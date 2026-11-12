# Writes the version header at build time:
#	BUILD    = days since 2000-01-01 (UTC)
#	REVISION = seconds since midnight (UTC) / 2
# Both fit the 16 bits of a version resource part; a later build always has a bigger version
#
# In: MAJOR, MINOR, SUFFIX, COMMIT, OUTPUT

string( TIMESTAMP nNow "%s" UTC )

# 10957 days from 1970-01-01 to 2000-01-01
math( EXPR nBuild "( ${nNow} / 86400 ) - 10957" )
math( EXPR nRevision "( ${nNow} % 86400 ) / 2" )

set( sText "${MAJOR}.${MINOR}.${nBuild}.${nRevision}${SUFFIX}" )
set( sProduct "${sText} (${COMMIT})" )

file( WRITE "${OUTPUT}.tmp"
"#ifndef HPPXDARK_VERSION_H
#define HPPXDARK_VERSION_H

#define HPPXDARK_VERSION_MAJOR		${MAJOR}
#define HPPXDARK_VERSION_MINOR		${MINOR}
#define HPPXDARK_VERSION_BUILD		${nBuild}
#define HPPXDARK_VERSION_REVISION	${nRevision}

#define HPPXDARK_VERSION_TEXT		\"${sText}\"
#define HPPXDARK_VERSION_TEXT_W		L\"${sText}\"
#define HPPXDARK_PRODUCT_VERSION	\"${sProduct}\"

#endif
" )

execute_process( COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OUTPUT}.tmp" "${OUTPUT}" )
file( REMOVE "${OUTPUT}.tmp" )
