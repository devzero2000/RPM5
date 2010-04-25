%define	sql_		/tmp/%name
%define	sql_db		%{sql_}/db
%define	sql_schema	%{sql_}/schema.sql
%define	sql_load	%{sql_}/load.sql
%define	sql_unload	%{sql_}/unload.sql
%define	sql_ins		%{sql_}/ins.sql
%define	sql_del		%{sql_}/del.sql
%define	sql_init	%{sql_}/init.sql
%define	sql_post	%{sql_}/post.sql
%define	sql_preun	%{sql_}/preun.sql

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
mkdir -p %{buildroot}%{sql_}/

touch %{buildroot}%{sql_db}

cat > %{buildroot}%{sql_schema} << EOF
CREATE TABLE IF NOT EXISTS
 Packages (
  i	INTEGER UNIQUE PRIMARY KEY NOT NULL,
  h	BLOB NOT NULL
);
CREATE TEMP TRIGGER insert_Packages
 AFTER INSERT ON Packages
  BEGIN
    INSERT INTO Nvra (k,v)	VALUES ( new.h, new.rowid );
  END;
CREATE TEMP TRIGGER delete_Packages
 BEFORE DELETE ON Packages
  BEGIN
    DELETE FROM Nvra WHERE v = old.rowid;
  END;

CREATE TABLE IF NOT EXISTS
 Nvra (
  k	TEXT PRIMARY KEY NOT NULL,
  v	INTEGER REFERENCES Packages(i) ON UPDATE RESTRICT ON DELETE RESTRICT
);
EOF

cat > %{buildroot}%{sql_load} << EOF
-- Load data.
BEGIN TRANSACTION;
INSERT into Packages (h) VALUES ('bing-1.2-3.noarch');
INSERT into Packages (h) VALUES ('bang-4.5-6.noarch');
INSERT into Packages (h) VALUES ('boom-7.8.9.noarch');
COMMIT TRANSACTION;
SELECT * from Nvra;
EOF

cat > %{buildroot}%{sql_unload} << EOF
-- Unload data.
BEGIN TRANSACTION;
DELETE from Packages WHERE h = 'bing-1.2-3.noarch';
DELETE from Packages WHERE h = 'bang-4.5-6.noarch';
DELETE from Packages WHERE h = 'boom-7.8.9.noarch';
COMMIT TRANSACTION;
SELECT * from Nvra;
EOF

cat > %{buildroot}%{sql_ins} << EOF
-- Basic rpmdbAdd() operation.
BEGIN TRANSACTION;
INSERT into Packages (h) VALUES ('popt-1.13-5.fc11.i586');
COMMIT TRANSACTION;
SELECT * FROM Nvra;
EOF

cat > %{buildroot}%{sql_del} << EOF
-- Basic rpmdbRemove() operation.
BEGIN TRANSACTION;
DELETE FROM Packages WHERE i = 2;
COMMIT TRANSACTION;
SELECT * from Nvra;
EOF

cat > %{buildroot}%{sql_init} << EOF
-- Configure the SQL parser parameters to known defaults.
.echo on
.explain off
.headers on
.mode list
.nullvalue ""
.prompt '%name> ' '---> '
.separator "|"
.timeout 1000
.timer off
.show
EOF

cat > %{buildroot}%{sql_post} << EOF
.read %{sql_init}
-- Create the schema and populate the database.
.read %{sql_schema}
.read %{sql_load}
.read %{sql_ins}
EOF

cat > %{buildroot}%{sql_preun} << EOF
.read %{sql_init}
-- De-populate the database.
.read %{sql_del}
.read %{sql_unload}
EOF

%post	-p "<sql> -init %{sql_post} %{sql_db}"
SELECT * FROM Packages;

%preun	-p "<sql> -init %{sql_preun} %{sql_db}"
SELECT * FROM Packages;

%verifyscript
echo "%%{sql -init /dev/null %{sql_db}:SELECT * FROM Packages;}"
echo "%%{sql -init /dev/null %{sql_db}:SELECT * FROM Nvra;}"

%files
%dir %{sql_}
%ghost %{sql_db}
%{sql_schema}
%{sql_load}
%{sql_unload}
%{sql_ins}
%{sql_del}
%{sql_init}
%{sql_post}
%{sql_preun}

%changelog
* Sun Apr 11 2010 Jeff Johnson <jbj@rpm5.org> - 1.0-1
- Embedded SQL functional tests.
