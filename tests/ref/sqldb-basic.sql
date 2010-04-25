-- Configure the SQL parser parameters to known defaults.
.echo on
.explain off
.headers on
-- .log stderr
.mode list
.nullvalue ""
.output stdout
.prompt 'foo> ' '---> '
.separator "|"
.timeout 1000
.timer off
.show

-- Instantiate the rpmdb.sql schema.
.read ref/rpmdb.sql

-- Basic rpmdbAdd/rpmdbRemove operations using SQL triggers.
BEGIN TRANSACTION;
INSERT into Packages (h) VALUES ('bing-1.2-3.noarch');
INSERT into Packages (h) VALUES ('bang-4.5-6.noarch');
INSERT into Packages (h) VALUES ('boom-7.8.9.noarch');
COMMIT TRANSACTION;

SELECT * from Packages;
SELECT * from Nvra;

BEGIN TRANSACTION;
DELETE FROM Packages WHERE i = 2;
COMMIT TRANSACTION;

SELECT * from Packages;
SELECT * from Nvra;

-- Basic .foo SQL metadata tests.
.backup main tmp/main.bak
.restore main tmp/main.bak
.dump main

.databases
.indices Packages
.schema
.tables

.exit
