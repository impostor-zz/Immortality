# -*- cmake -*-

if (STANDALONE)
    set(LIBGCRYPT_FIND_REQUIRED ON)
    include(FindLibgcrypt)
else (STANDALONE)
    set(LIBGCRYPT_INCLUDE_DIR ../libgcrypt/libgcrypt-1.2.2/src)
    set(LIBGCRYPT_LIBRARIES libgcrypt)
endif (STANDALONE)

if (WINDOWS)
  set(LIBGCRYPT_INCLUDE_DIRS
      ${LIBGCRYPT_INCLUDE_DIR}
      ../gpg.vs/inc.vs
      ../gpg.vs/libgcrypt-1.2.2.vs/custom
      )
else (WINDOWS)
  set(LIBGCRYPT_INCLUDE_DIRS ${LIBGCRYPT_INCLUDE_DIR})
endif (WINDOWS)
