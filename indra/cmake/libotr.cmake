# -*- cmake -*-

include(libgpg-error)
include(libgcrypt)

if (STANDALONE)
    set(LIBOTR_LIBRARIES libotr)
else (STANDALONE)
    include(Prebuilt)
    
    if (WINDOWS)
	set(LIBOTR_LIBRARIES libotr)
    else (WINDOWS)
	use_prebuilt_binary(otr)
	set(LIBOTR_LIBRARIES otr)
    endif (WINDOWS)
endif (STANDALONE)

set(LIBOTR_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/libotr/libotr-3.2.0/src
    ${LIBGPG_ERROR_INCLUDE_DIRS}
    ${LIBGCRYPT_INCLUDE_DIRS}
    )


