<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Runtime document loader

Original Wirescape graph-building code, GPL-3.0-only. Qt's document migration,
validation and atomic writer remain authoritative for editable/preserved data.
The Qt app and Qt-free runner share `runtime_patch.cpp` for graph construction.
Parsing/graph creation happen quiescently; no JSON library enters the callback.
Input is limited to 2 MiB, depth 64, 64 modules and 128 cables. Duplicate object
keys, malformed types, unknown v2 parameters and unavailable modules fail closed.

JSON dependency: nlohmann/json v3.11.3, commit
`9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`, MIT licence, upstream
https://github.com/nlohmann/json. FetchContent pins the commit archive and SHA256;
no third-party source is vendored or changed. Licence: upstream LICENSE.MIT.
