# Historical development material

The PR candidate keeps production sources, maintainable tests, board defaults and
one self-contained five-board dataset. Historical logs, complete sdkconfig files,
source snapshots, driver observer copies, experiments and dated reports are
excluded. Original paths, sizes and SHA-256 are in [archive-index.json](archive-index.json).

The original checkout still holds this material. Before submitting the PR, store a
snapshot in project-approved persistent storage and add its accessible URL and
archive hash here. A local filesystem path is not a shared artifact URL.

Historical paths in final source-index.json and measurement records refer to that
snapshot; they are not rewritten to suggest current code was retested.
[The final report](evidence/display-latest-20261111/README.md) can be rendered without
historical logs or a firmware build. No test measurement is changed by this cleanup.

Unreleased SDK capability-query patches are historical material in the external
snapshot, not a production dependency or part of this PR.
