# Perl specific macro definitions.
# To make use of these macros insert the following line into your spec file:
# %include /opt/local/lib/rpm/macros.php

%define		__find_requires	/opt/local/lib/rpm/find-php-requires
%define		__find_provides	/opt/local/lib/rpm/find-php-provides

%define		php_pear_dir	%{_datadir}/pear

