# Changelog: v2.0

## Review dispositions
Accepted: 12. Modified: 8. Rejected: 1. (Full table: `docs/review-history.md`, Section 31.)

## Top changes
- Plan split: a short master (`LUMORA_MASTER.md`) plus 19 normative files under `docs/`; history in `docs/review-history.md` with message identifiers, timestamps and quotes removed; inline version tags removed from normative text.
- Decisions D1-D31 unchanged in meaning. Only message identifiers, timestamps and quotes were removed from the decision log. A private file keeps the original wording for the owner. No "owner approved" claim was added, and the review's recommended answers stay open questions.
- New milestone M3S (vertical slice after M3). Effort re-added: 80-135 weeks, about 18-31 months (was 71-120 weeks). The increase is judgment estimates for the slice, authoring work, CI latency and larger M2 and M5.
- Security fixes: "kill and restart the logic thread" replaced (bionic has no `pthread_cancel`); own wallpaper process, crash-loop breaker and isolated import validation; static shader limits; microphone pause versus stop.
- Stale text fixed: sandbox allowlist wording, closed RHI question, duplicated video sentence, GNOME decision wording.
- Added: continuity protocol and `PROJECT_STATE.md` template, authoring-experience work item, capability tiers, Lua API additions, reduced motion, bidi library choice (SheenBidi), spike success criteria, GPL provenance rule, CI fast and slow split, secret scanning.
- Added `tools/check_docs.py`, an automated consistency check.
