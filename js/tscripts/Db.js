if (loglvl) print("--> Db.js");

var db = new Db();
ack("typeof db;", "object");
ack("db instanceof Db;", true);
ack("db.debug = 1;", 1);
ack("db.debug = 0;", 0);

print("home:	"+db.home);
print("open_flags:	"+db.open_flags);
print("data_dirs:	"+db.data_dirs);
print("create_dir:	"+db.create_dir);
print("encrypt_flags:	"+db.encrypt_flags);
print("errfile:	"+db.errfile);
print("errpfx:	"+db.errpfx);
print("flags:	"+db.flags);
print("idirmode:	"+db.idirmode);
print("msgfile:	"+db.msgfile);
print("shm_key:	"+db.shm_key);
print("thread_count:	"+db.thread_count);
print("timeout:	"+db.timeout);
print("tmp_dir:	"+db.tmp_dir);
print("verbose:	"+db.verbose);
print("lk_conflicts:	"+db.lk_conflicts);
print("lk_detect:	"+db.lk_detect);
print("lk_max_lockers: "+db.lk_max_lockers);
print("lk_max_locks:	"+db.lk_max_locks);
print("lk_max_objects:	"+db.lk_max_objects);
print("lk_partitions:	"+db.lk_partitions);
print("log_config:	"+db.log_config);
print("lg_bsize:	"+db.lg_bsize);
print("lg_dir:	"+db.lg_dir);
print("lg_filemode:	"+db.lg_filemode);
print("lg_max:	"+db.lg_max);
print("lg_regionmax:	"+db.lg_regionmax);

if (loglvl) print("<-- Db.js");
