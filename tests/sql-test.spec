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
CREATE TABLE IF NOT EXISTS Packages (
  v     INTEGER UNIQUE PRIMARY KEY NOT NULL,
  k     BLOB NOT NULL
);
CREATE TRIGGER insert_Packages AFTER INSERT ON Packages
  BEGIN
    INSERT INTO Nvra (k,v)      VALUES (
        new.k, new.rowid );
    INSERT INTO Name (k,v)      VALUES (
        SUBSTR(new.k,  1, 4), new.rowid );
    INSERT INTO Version (k,v)   VALUES (
        SUBSTR(new.k,  6, 3), new.rowid );
    INSERT INTO Release (k,v)   VALUES (
        SUBSTR(new.k, 10, 1), new.rowid );
    INSERT INTO Arch (k,v)      VALUES (
        SUBSTR(new.k, 12), new.rowid );
  END;
CREATE TRIGGER delete_Packages BEFORE DELETE ON Packages
  BEGIN
    DELETE FROM Nvra    WHERE v = old.rowid;
    DELETE FROM Name    WHERE v = old.rowid;
    DELETE FROM Version WHERE v = old.rowid;
    DELETE FROM Release WHERE v = old.rowid;
    DELETE FROM Arch    WHERE v = old.rowid;
  END;
CREATE TABLE IF NOT EXISTS Nvra (
  k     TEXT NOT NULL,
  v     INTEGER REFERENCES Packages
);
CREATE TABLE IF NOT EXISTS Name (
  k     TEXT NOT NULL,
  v     INTEGER REFERENCES Packages
);
CREATE TABLE IF NOT EXISTS Version (
  k     TEXT NOT NULL,
  v     INTEGER REFERENCES Packages
);
CREATE TABLE IF NOT EXISTS Release (
  k     TEXT NOT NULL,
  v     INTEGER REFERENCES Packages
);
CREATE TABLE IF NOT EXISTS Arch (
  k     TEXT NOT NULL,
  v     INTEGER REFERENCES Packages
);
EOF

cat > %{buildroot}%{sql_load} << EOF
-- Load data.
BEGIN TRANSACTION;
INSERT into Packages (k) VALUES ('bing-1.2-3.noarch');
INSERT into Packages (k) VALUES ('bang-4.5-6.noarch');
INSERT into Packages (k) VALUES ('boom-7.8-9.noarch');
COMMIT TRANSACTION;
SELECT * from Nvra;
EOF

cat > %{buildroot}%{sql_unload} << EOF
-- Unload data.
BEGIN TRANSACTION;
DELETE from Packages WHERE k = 'bing-1.2-3.noarch';
DELETE from Packages WHERE k = 'bang-4.5-6.noarch';
DELETE from Packages WHERE k = 'boom-7.8.9.noarch';
COMMIT TRANSACTION;
SELECT * from Nvra;
EOF

cat > %{buildroot}%{sql_ins} << EOF
-- Basic rpmdbAdd() operation.
BEGIN TRANSACTION;
INSERT into Packages (k) VALUES ('flim-3.2-1.i586');
COMMIT TRANSACTION;
SELECT * FROM Nvra;
EOF

cat > %{buildroot}%{sql_del} << EOF
-- Basic rpmdbRemove() operation.
BEGIN TRANSACTION;
DELETE FROM Packages WHERE v = 2;
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

# XXX undo tests/macros automatic wrapping
%undefine post
%undefine preun
%undefine verifyscript

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
