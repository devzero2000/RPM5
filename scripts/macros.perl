# Perl specific macro definitions.
# To make use of these macros insert the following line into your spec file:
# %include /opt/local/lib/rpm/macros.perl

%define		__find_requires	/opt/local/lib/rpm/find-perl-requires
%define		__find_provides	/opt/local/lib/rpm/find-perl-provides

%define		perl_sitelib	%(eval "`perl -V:installsitelib`"; echo $installsitelib)
%define		perl_sitearch	%(eval "`perl -V:installsitearch`"; echo $installsitearch)
%define		perl_archlib	%(eval "`perl -V:installarchlib`"; echo $installarchlib)
%define		perl_privlib	%(eval "`perl -V:installprivlib`"; echo $installprivlib)

