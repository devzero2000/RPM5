/*
**  db_tool.c -- integrated Berkeley-DB tools to reduce bloat on installation files
*/

#include <stdio.h>
#include <string.h>

/* integrate db_stat */
#define main          db_stat_main
#define db_init       db_stat_db_init
#define copyright     db_stat_copyright
#define usage         db_stat_usage
#define version_check db_stat_version_check
int db_stat_main(int argc, char *argv[]);
#include "db_stat/db_stat.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_load */
#define main          db_load_main
#define db_init       db_load_db_init
#define copyright     db_load_copyright
#define usage         db_load_usage
#define version_check db_load_version_check
int db_load_main(int argc, char *argv[]);
#include "db_load/db_load.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_dump */
#define main          db_dump_main
#define db_init       db_dump_db_init
#define copyright     db_dump_copyright
#define usage         db_dump_usage
#define version_check db_dump_version_check
int db_dump_main(int argc, char *argv[]);
#include "db_dump/db_dump.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_archive */
#define main          db_archive_main
#define db_init       db_archive_db_init
#define copyright     db_archive_copyright
#define usage         db_archive_usage
#define version_check db_archive_version_check
int db_archive_main(int argc, char *argv[]);
#include "db_archive/db_archive.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_checkpoint */
#define main          db_checkpoint_main
#define db_init       db_checkpoint_db_init
#define copyright     db_checkpoint_copyright
#define usage         db_checkpoint_usage
#define version_check db_checkpoint_version_check
int db_checkpoint_main(int argc, char *argv[]);
#include "db_checkpoint/db_checkpoint.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_deadlock */
#define main          db_deadlock_main
#define db_init       db_deadlock_db_init
#define copyright     db_deadlock_copyright
#define usage         db_deadlock_usage
#define version_check db_deadlock_version_check
int db_deadlock_main(int argc, char *argv[]);
#include "db_deadlock/db_deadlock.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_recover */
#define main          db_recover_main
#define db_init       db_recover_db_init
#define copyright     db_recover_copyright
#define usage         db_recover_usage
#define version_check db_recover_version_check
int db_recover_main(int argc, char *argv[]);
#include "db_recover/db_recover.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_upgrade */
#define main          db_upgrade_main
#define db_init       db_upgrade_db_init
#define copyright     db_upgrade_copyright
#define usage         db_upgrade_usage
#define version_check db_upgrade_version_check
int db_upgrade_main(int argc, char *argv[]);
#include "db_upgrade/db_upgrade.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_verify */
#define main          db_verify_main
#define db_init       db_verify_db_init
#define copyright     db_verify_copyright
#define usage         db_verify_usage
#define version_check db_verify_version_check
int db_verify_main(int argc, char *argv[]);
#include "db_verify/db_verify.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_hotbackup */
#define main          db_hotbackup_main
#define db_init       db_hotbackup_db_init
#define copyright     db_hotbackup_copyright
#define usage         db_hotbackup_usage
#define version_check db_hotbackup_version_check
int db_hotbackup_main(int argc, char *argv[]);
#include "db_hotbackup/db_hotbackup.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_printlog */
#define main          db_printlog_main
#define db_init       db_printlog_db_init
#define copyright     db_printlog_copyright
#define usage         db_printlog_usage
#define version_check db_printlog_version_check
int db_printlog_main(int argc, char *argv[]);
#include "db_printlog/db_printlog.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check

/* integrate db_svc */
#ifdef WITH_DB_RPC
#define main          db_svc_main
#define db_init       db_svc_db_init
#define copyright     db_svc_copyright
#define usage         db_svc_usage
#define version_check db_svc_version_check
int db_svc_main(int argc, char *argv[]);
#include "rpc_server/c/db_server_util.c"
#undef  main
#undef  copyright
#undef  db_init
#undef  usage
#undef  version_check
#endif

/* tool dispatch table */
static struct {
    char *name;
    int (*main)(int, char *[]);
} main_dispatch[] = {
#ifdef WITH_DB_RPC
    { "db_svc",        db_svc_main        },
#endif
    { "db_stat",       db_stat_main       },
    { "db_load",       db_load_main       },
    { "db_dump",       db_dump_main       },
    { "db_archive",    db_archive_main    },
    { "db_checkpoint", db_checkpoint_main },
    { "db_deadlock",   db_deadlock_main   },
    { "db_recover",    db_recover_main    },
    { "db_upgrade",    db_upgrade_main    },
    { "db_verify",     db_verify_main     },
    { "db_hotbackup",  db_hotbackup_main  },
    { "db_printlog",   db_printlog_main   }
};

/* dispatch tools */
int main(int argc, char *argv[])
{
    int i;
    char *arg;
    int l;
    int k;

    /* 1. try to dispatch over program name ("db_load [...]") */
    arg = argv[0];
    l = strlen(arg);
    for (i = 0; i < sizeof(main_dispatch)/sizeof(main_dispatch[0]); i++) {
        k = strlen(main_dispatch[i].name);
        if (   strcmp(arg, main_dispatch[i].name) == 0
            || (l > k && arg[l-k-1] == '/' && strcmp(arg+l-k, main_dispatch[i].name) == 0)) {
            argv[0] = main_dispatch[i].name;
            return (*(main_dispatch[i].main))(argc, argv);
        }
    }

    /* 2. try to dispatch over sub-command name ("db_tool [db_]load [...]") */
    if (argc >= 2) {
        arg = argv[1];
        l = strlen(arg);
        for (i = 0; i < sizeof(main_dispatch)/sizeof(main_dispatch[0]); i++) {
            k = strlen(main_dispatch[i].name);
            if (   strcmp(argv[1], main_dispatch[i].name) == 0
                || (l == k-3 && strcmp(arg, main_dispatch[i].name+3) == 0)) {
                argv[0] = main_dispatch[i].name;
                for (i = 3; i <= argc; i++)
                    argv[i-2] = argv[i-1];
                argc--;
                return (*(main_dispatch[i].main))(argc, argv);
            }
        }
    }

    /* 3. fail to dispatch */
    fprintf(stderr, "db_tool:ERROR: unable to determine command name\n");
    fprintf(stderr, "db_tool:USAGE: db_<command> <options>\n");
    fprintf(stderr, "db_tool:USAGE: db_tool db_<command> <options>\n");
    fprintf(stderr, "db_tool:USAGE: db_tool <command> <options>\n");
    fprintf(stderr, "db_tool:HINT: <command> is one of:");
    for (i = 0; i < sizeof(main_dispatch)/sizeof(main_dispatch[0]); i++)
        fprintf(stderr, " %s", main_dispatch[i].name+3);
    fprintf(stderr, "\n");
    return 1;
}

