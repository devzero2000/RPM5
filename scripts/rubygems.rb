#!/usr/bin/env ruby
#--
# Copyright 2010 Per Øyvind Karlsen <peroyvind@mandriva.org>
# This program is free software. It may be redistributed and/or modified under
# the terms of the LGPL version 2.1 (or later).
#++

require 'optparse'
require 'rubygems'

provides = false
requires = false

opts = OptionParser.new("#{$0} <--provides|--requires>")
opts.on("-P", "--provides", "Print provides") do |val|
    provides = true
end
opts.on("-R", "--requires", "Print requires") do |val|
    requires= true
end

rest = opts.permute(ARGV)

if rest.size != 0 or (!provides and !requires) or (provides and requires)
    $stderr.puts "Use either --provides OR --requires"
    $stderr.puts opts
    exit(1)
end

specpath = "%s/specifications/.*\.gemspec$" % Gem::dir
gems = []
for gemspec in $stdin.readlines
  if gemspec.match(specpath)
    gems.push(gemspec.chomp)
  end
end
if gems.length > 0
  if requires
    require 'rbconfig'

    module Gem
      class Requirement
        def rpm_dependency_transform(name, version)
          pessimistic = ""
          if version == "> 0.0.0" or version == ">= 0"
            version = ""
          else
            if version[0..1] == "~>"
              pessimistic = "rubygem(%s) < %s\n" % [name, version[3..-1]]
              pessimistic[-2] = pessimistic[-2] + 1
              version = version.gsub(/\~>/, '=>')
            end
            version = version.gsub(/^/, ' ')
          end
          version = "rubygem(%s)%s\n%s" % [name, version, pessimistic]
        end

        def to_rpm(name)
          result = as_list
          return result.map { |version| rpm_dependency_transform(name, version) }
        end

      end
    end
    print "ruby >= %s\n" % Config::CONFIG["ruby_version"]
  end

  for gem in gems
    data = File.read(gem)
    spec = eval(data)
    if provides
      print "rubygem(%s) = %s\n" % [spec.name, spec.version]
    end
    if requires
      for d in spec.dependencies
        print d.version_requirements.to_rpm(d.name)
      end
      for d in spec.required_rubygems_version.to_rpm("rubygems")
        print d.gsub(/(rubygem\()|(\))/, "")
      end
    end
  end
end
