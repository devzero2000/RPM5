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
	ix		INTEGER,
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Basenames( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	bn		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Group(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	G		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Providename( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Requirename( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Conflictname( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Obsoletename( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Triggername( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	N		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Dirnames(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	dn		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Requireversion( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	EVR		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Provideversion( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	EVR		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Installtid( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	TID		INTEGER PRIMARY KEY
);

CREATE TABLE _Sigmd5(	/*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	MD5		BIN(16) PRIMARY KEY
);

CREATE TABLE _Sha1header( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	SHA1		VARCHAR2(41) PRIMARY KEY
);

CREATE TABLE _Filedigests( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	digest		BIN(16) PRIMARY KEY
);

CREATE TABLE _Pubkeys(	/*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	fingerprint	BIN(8) PRIMARY KEY
);

CREATE TABLE _Packagecolor( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	color		INTEGER PRIMARY KEY
);

CREATE TABLE _Nvra(	/*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	NVRA		VARCHAR2(64) PRIMARY KEY
);

CREATE TABLE _Sourcepkgid( /*+ DBTYPE = BTREE */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	MD5		BIN(16) PRIMARY KEY
);

CREATE TABLE _Filepaths( /*+ DBTYPE = HASH */
	hx		INTEGER REFERENCES Packages(instance),
	ix		INTEGER,
	fn		VARCHAR2(64) PRIMARY KEY
);
