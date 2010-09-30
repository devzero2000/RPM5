Summary:	Test Package for testing changing directory to symlink on upgrade
Name:		test-change-dir-to-symlink
Version:	1
Release:	2
License:	LGPL
Group:		AAmusements/Games
BuildRoot:	%{_tmppath}/%{name}-%{version}-root-%(id -u -n)
URL:		http://www.rpm5.org
BuildArch:      noarch

%description
This is the second package release. It replace
a /tmp/%{name} directory in upgrade with 
a symbolic link to /tmp/%{name}-directory

%prep
#empty

%build
#empty

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/tmp/%{name}-directory
echo "hello" > %{buildroot}/tmp/%{name}-directory/%{name}-file
echo "I can fix this issue" > %{buildroot}/tmp/%{name}-directory/%{name}-file-1

ln -s  /tmp/%{name}-directory %{buildroot}/tmp/%{name}

#empty 

%pretrans  -p <lua>
local leaf_indent = '|   '
local tail_leaf_indent = '    '
local leaf_prefix = '|-- '
local tail_leaf_prefix = '`-- '
local link_prefix = ' -> '

local function printf(...)
    io.write(string.format(unpack(arg)))
end

local function rm_directory(directory, level, prefix)
    local num_dirs = 0
    local num_files = 0
    if  posix.access(directory,"rw") then
    local files = posix.dir(directory)
    local last_file_index = table.getn(files)
    table.sort(files)
    for i, name in ipairs(files) do
        if name ~= '.' and name ~= '..' then
            local full_name = string.format('%s/%s', directory, name)
            local info = assert(posix.stat(full_name))
            local is_tail = (i==last_file_index)
            local prefix2 = is_tail and tail_leaf_prefix or leaf_prefix
            local link = ''
            if info.type == 'link' then
                linked_name = assert(posix.readlink(full_name))
                link = string.format('%s%s', link_prefix, linked_name)
                posix.unlink(full_name)
            end
            printf('%s%s%s%s\n', prefix, prefix2, name, link)
            if info.type == 'directory' then
                local indent = is_tail and tail_leaf_indent or leaf_indent
                sub_dirs, sub_files = rm_directory(full_name, level+1,
                    prefix .. indent)
                num_dirs = num_dirs + sub_dirs + 1
                num_files = num_files + sub_files
                posix.rmdir(full_name)
            else
                print(posix.unlink(full_name))
                num_files = num_files + 1
            end
        end
    end
    end -- if access
    return num_dirs, num_files
end

local function call_rm_directory(directory)
    print(directory)
    num_dirs, num_files = rm_directory(directory, 0, '')
    printf('\ndropped %d directories, %d files\n', num_dirs, num_files)
    posix.rmdir(directory)
end
call_rm_directory("/tmp/%{name}")


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
/tmp/%{name}*

%changelog
* Mon Aug 4 2010 Elia Pinto <devzero2000@rpm5.org> 1-2
- change /tmp/%{name} dir to a symbolic link
  to /tmp/%{name}-directory
