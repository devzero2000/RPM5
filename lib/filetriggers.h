#ifndef H_FILETRIGGERS
#define H_FILETRIGGERS

/**
 * \file lib/filetriggers.h
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

__attribute__ ((visibility("hidden")))
  int mayAddToFilesAwaitingFiletriggers(const char *rootDir, rpmfi fi, int install_or_erase);

void rpmRunFileTriggers(const char *rootDir);

#ifdef __cplusplus
}
#endif

#endif	/* H_FILETRIGGERS */
