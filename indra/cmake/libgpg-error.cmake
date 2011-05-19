# -*- cmake -*-

if (STANDALONE)
    set(LIBGPG_ERROR_FIND_REQUIRED ON)
    include(FindLibgpg_error)
else (STANDALONE)
    set(LIBGPG_ERROR_INCLUDE_DIR ../libgpg-error/libgpg-error-1.0/src)
    set(LIBGPG_ERROR_LIBRARIES libgpg-error)
endif (STANDALONE)

if (WINDOWS)
  set(LIBGPG_ERROR_INCLUDE_DIRS
      ${LIBGPG_ERROR_INCLUDE_DIR}
      ../gpg.vs/inc.vs
      ../gpg.vs/libgpg-error-1.1.vs/custom
      )
else (WINDOWS)
  set(LIBGPG_ERROR_INCLUDE_DIRS ${LIBGPG_ERROR_INCLUDE_DIR})
endif (WINDOWS)
