Summary:   Embedded SQL test case.
Name:      sql-test
Version:   1.0
Release:   1
License:   Public Domain
Group:     Development/Tests
URL:       http://rpm5.org/
Prefix:    /tmp
BuildArch: noarch

%description			# <== NUKE

%install
mkdir -p %{buildroot}/tmp/%{name}

cat > %{buildroot}/tmp/%{name}/sqldb-init.sql << EOF
-- Configure the SQL parser parameters to known defaults.
.echo on
.explain off
.headers on
.mode list
.nullvalue ""
.prompt 'foo> ' '---> '
.separator "|"
.timeout 1000
.timer off
.show

-- Instantiate the sqldb.sql schema.
.read /tmp/%{name}/sqldb-create.sql

-- Basic rpmdbAdd/rpmdbRemove operations using SQL triggers.
BEGIN TRANSACTION;
INSERT into Packages (h) VALUES ('bing-1.2-3.noarch');
INSERT into Packages (h) VALUES ('bang-4.5-6.noarch');
INSERT into Packages (h) VALUES ('boom-7.8.9.noarch');
COMMIT TRANSACTION;

.exit
EOF

cat > %{buildroot}/tmp/%{name}/sqldb-create.sql << EOF
CREATE TABLE Packages (
  i	INTEGER UNIQUE PRIMARY KEY NOT NULL,
  h	BLOB NOT NULL
);
CREATE TEMP TRIGGER insert_Packages AFTER INSERT ON Packages
  BEGIN
    INSERT INTO Nvra (k,v)	VALUES ( new.h, new.rowid );
  END;
CREATE TEMP TRIGGER delete_Packages BEFORE DELETE ON Packages
  BEGIN
    DELETE FROM Nvra WHERE v = old.rowid;
  END;

CREATE TABLE Nvra (
  k	TEXT PRIMARY KEY NOT NULL,
  v	INTEGER REFERENCES Packages(i) ON UPDATE RESTRICT ON DELETE RESTRICT
);
EOF

cat > %{buildroot}/tmp/%{name}/sqldb-load.sql << EOF
-- Basic rpmdbAdd/rpmdbRemove operations using SQL triggers.
BEGIN TRANSACTION;
INSERT into Packages (h) VALUES ('bing-1.2-3.noarch');
INSERT into Packages (h) VALUES ('bang-4.5-6.noarch');
INSERT into Packages (h) VALUES ('boom-7.8.9.noarch');
COMMIT TRANSACTION;

SELECT * FROM Packages;
SELECT * FROM Nvra;

BEGIN TRANSACTION;
DELETE FROM Packages WHERE i = 2;
COMMIT TRANSACTION;

SELECT * from Packages;
SELECT * from Nvra;

EOF

%post -p "<sql> -init /tmp/%{name}/sqldb-init.sql /tmp/%{name}/sqldb"
SELECT * FROM Packages;
SELECT * FROM Nvra;

%files
%dir /tmp/%{name}
/tmp/%{name}/sqldb-init.sql
/tmp/%{name}/sqldb-create.sql
/tmp/%{name}/sqldb-load.sql

%changelog
* Sun Apr 11 2010 Jeff Johnson <jbj@rpm5.org> - 1.0-1
- Embedded SQL functional tests.
