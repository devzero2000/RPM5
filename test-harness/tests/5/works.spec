Summary:	The rpm that simply works.
Name: 		works
Version:	1
Release:	2
Group: 		System Environment/Base
Copyright:	GPL

%description
It just works.  What more do you want?

%pre
rm -f /tmp/install_pre_sema_not_exist
[ -f "%{semaphore_backout}" ]   && echo "   pre:  Backout semaphore exists!" 
if [ ! -f "%{semaphore_backout}" ] 
then
	echo "   pre:  Backout semaphore does not exist..." 
	touch /tmp/install_pre_sema_not_exist
fi
exit 0

%post
rm -f /tmp/install_post_sema_not_exist
[ -f "%{semaphore_backout}" ]   && echo "  post:  Backout semaphore exists!"
if [ ! -f "%{semaphore_backout}" ] 
then
	echo "  post:  Backout semaphore does not exist..."
	touch /tmp/install_post_sema_not_exist
fi
	
exit 0

%files
