SQLCipher Community 4.19.0

Upstream: https://github.com/sqlcipher/sqlcipher/tree/v4.19.0
Commit: c4b275a47932888216bade83aff2bbc73df0ff85
Files sqlite3.c and sqlite3.h contain upstream amalgamations, with CRLF line endings, generated
with `nmake /f Makefile.msc sqlite3.c sqlite3.h NO_TCL=1`.
License: SQLCIPHER-LICENSE.md (BSD 3-clause).
Crypto provider: existing OpenSSL dependency. Required compile definitions
are recorded per source in Telegram/CMakeLists.txt.
