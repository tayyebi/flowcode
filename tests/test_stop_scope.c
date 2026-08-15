/*
 * Regression test: `stop` must end only the branch it appears in. The
 * unreachable-code check must reset at the next `step` and at the next
 * `match` arm (`name ->`), not only at `step` — otherwise a `stop` inside
 * one match arm silently discards every step for the rest of the file.
 */
#include "fc_compiler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int has_unreachable_warning(const fc_compile_result_t *r) {
    size_t i;
    for (i = 0; i < r->diagnostic_count; i++) {
        if (strstr(r->diagnostics[i].message, "unreachable code after stop") != NULL) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    fc_compile_result_t result;

    printf("--- test_stop_scope ---\n");

    /* A stop inside one match arm must not swallow a step in a later,
     * unrelated arm, nor a step written after the match entirely. */
    {
        const char *src =
            "workflow: StopScope\n"
            "\n"
            "step start:\n"
            "    emit\n"
            "        value = \"go\"\n"
            "end\n"
            "\n"
            "match start.status\n"
            "    rejected ->\n"
            "        step reject:\n"
            "            emit\n"
            "                value = \"rejected\"\n"
            "            end\n"
            "        stop\n"
            "    approved ->\n"
            "        step approve:\n"
            "            emit\n"
            "                value = \"approved\"\n"
            "        end\n"
            "end\n"
            "\n"
            "step after:\n"
            "    store set\n"
            "        key = \"done\"\n"
            "end\n";

        result = fc_compile(src);
        assert(!result.has_errors);
        assert(!has_unreachable_warning(&result));

        /* 4 instructions: the leading emit, the match's route, the
         * "approved" arm's emit (reached — not swallowed by the earlier
         * stop), and the trailing store. The "rejected" arm's emit is also
         * present since match currently falls through every arm; what this
         * test guards is that "approve" and "after" are not dropped. */
        {
            uint32_t count;
            memcpy(&count, result.bytecode + 8, sizeof(count));
            assert(count >= 4);
        }
        printf("  PASS: stop in one match arm does not discard a later arm or step\n");
        fc_compile_result_free(&result);
    }

    /* A stop with nothing after it in the file is still fine: no warning,
     * because there is genuinely nothing unreachable to report. */
    {
        const char *src =
            "workflow: TrailingStop\n"
            "\n"
            "step only:\n"
            "    emit\n"
            "        value = \"x\"\n"
            "    stop\n"
            "end\n";

        result = fc_compile(src);
        assert(!result.has_errors);
        assert(!has_unreachable_warning(&result));
        printf("  PASS: a stop with nothing after it warns nothing\n");
        fc_compile_result_free(&result);
    }

    /* The case the bug actually broke: unreachable code genuinely after a
     * stop, with no step/arm boundary before it, must still warn. This
     * confirms the fix narrowed the reset points without disabling the
     * check entirely. */
    {
        const char *src =
            "workflow: GenuinelyUnreachable\n"
            "\n"
            "step only:\n"
            "    emit\n"
            "        value = \"x\"\n"
            "    stop\n"
            "    emit\n"
            "        value = \"never runs\"\n"
            "end\n";

        result = fc_compile(src);
        assert(!result.has_errors);
        assert(has_unreachable_warning(&result));
        printf("  PASS: code after stop with no boundary still warns\n");
        fc_compile_result_free(&result);
    }

    printf("--- all stop-scope tests passed ---\n");
    return 0;
}
