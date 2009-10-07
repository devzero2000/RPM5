#ifndef H_LOGIO
#define	H_LOGIO

#include "logio_auto.h"

int logio_Mkdir_log
    __P((DB_ENV *, DB_TXN *, DB_LSN *, uint32_t, const DBT *));
int logio_Mkdir_print
    __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
int logio_Mkdir_read
    __P((DB_ENV *, void *, logio_Mkdir_args **));
int logio_Mkdir_recover
    __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
int logio_init_print __P((DB_ENV *, DB_DISTAB *));

#endif /* !H_LOGIO */
