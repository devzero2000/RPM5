require 'librpmrb'

# RPM bindings module.
#
# This module directly houses several constants and enums used in the
# lower-level part of the bindings. Constants are reflected as ruby constants,
# enums as hashes.
#
# The Ruby bindings consist of two parts: First the direct bindings, which all
# have abbreviated names, e.g. RPM::Mc. These try to be as close to the
# original RPM API as possible. They also make no assumptions about parameters
# and generally have no defaults, as long as the bound RPM function doesn't
# make them. Second, a set of convenience methods/objects. These try to be
# closer to the "Ruby Way" in regards of the design. They're the ones with the
# written-out names, e.g. RPM::MacroContext. It should normally suffice to use
# the convenience methods and is generally preferred.
module RPM

  # These are the flags that drive the build process. The original definition
  # can be found in the build/rpmbuild.h file, along with explainations for
  # the different flags.
  # Generally, they should be self-explaining: :prep, :build, :install, 
  # :check, :clean and :track execute the associated scriptlets in the spec
  # file. :filecheck checks the %files sections, and :packagesource and
  # :packagebinary finally create the source .rpm and binary .rpm(s).
  # :rmsource, :rmbuild and :rmspec remove files after the build process, i.e.
  # the source files and patch, the build tree or the spec file itself.
  # Finally, :fetchsource downloads the source files given in the spec file.
  BUILD = {
    :none => 0,
    :prep => 1<<0,
    :build => 1<<1,
    :install => 1<<2,
    :check => 1<<3,
    :clean => 1<<4,
    :filecheck => 1<<5,
    :packagesource => 1<<6,
    :packagebinary => 1<<7,
    :rmsource => 1<<8,
    :rmbuild => 1<<9,
    :track => 1<<11,
    :rmspec => 1<<12,
    :fetchsource => 1<<13
  }

  TAG = {
    :name => 1000
  }
end
