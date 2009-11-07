CREATE DATABASE rpmdb;	/*+ CACHESIZE = 16m */

CREATE TABLE Packages(	/*+ DBTYPE = BTREE */
	ntags		INTEGER,
	ndata		INTEGER,
	tags		BIN(16),
	_data		BIN(1),
	instance	INTEGER UNIQUE PRIMARY KEY
);

CREATE TABLE _Name(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Providename( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Requirename( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Conflictname( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Triggername( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Dirnames(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	dn		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Installtid( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	TID		INTEGER PRIMARY KEY
);

CREATE TABLE _Sigmd5(	/*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	MD5		BIN(16) PRIMARY KEY
);

CREATE TABLE _Sha1header( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	SHA1		VARCHAR2(41) PRIMARY KEY
);

CREATE TABLE _Filedigests( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	digest		BIN(16) PRIMARY KEY
);

CREATE TABLE _Pubkeys(	/*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	fingerprint	BIN(8) PRIMARY KEY
);

CREATE TABLE _Packagecolor( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	color		INTEGER PRIMARY KEY
);

CREATE TABLE _Nvra(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	NVRA		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Sourcepkgid( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	MD5		BIN(16) PRIMARY KEY
);

CREATE TABLE _Filepaths( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	fn		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Group(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	G		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Obsoletename( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	N		VARCHAR2(64) PRIMARY KEY
);
