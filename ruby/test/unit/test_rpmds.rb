require 'test/unit'
require 'rpm'
require 'fileutils'
require 'tempfile'
require 'pathname'
require 'tmpdir'


class TestRPMDS < Test::Unit::TestCase
  def setup
    @fixture_path = File.expand_path File.dirname(__FILE__) + '/../fixtures'
    @macros = [
      File.expand_path(File.dirname(__FILE__) + '/../../../macros/macros'),
      File.expand_path(File.dirname(__FILE__) + 
          '/../../../macros/macros.rpmbuild') ]

    @tmpfile = Tempfile.new 'rpmtest'
    @tmpdir = Dir.mktmpdir

    gmc = RPM::Mc.global_context
    gmc.init_macros(@macros.join ':')
    gmc.add '_sourcedir ' + @fixture_path
    gmc.add '_builddir ' + @tmpdir
    gmc.add '_rpmdir ' + @tmpdir
    gmc.add '_srcrpmdir ' + @tmpdir
    gmc.add 'tmpfile ' + @tmpfile.path
  end

  def test_ds_from_spec
    ts = RPM::Ts.new
    spec = ts.parse_spec File.expand_path(
      File.dirname(__FILE__) + '/../fixtures/multi_package_mock.spec'),
        # rootURL, recursing
        '/', false,
        # passphrase, cookie, anyarch, force, verify
      '', nil, false, false, true

    headers = spec.binheaders
    assert_equal 3, headers.length, 'headers array must contain 3 headers.'

    assert_equal 'P mainpkg = 0:1.0-1.0',
      headers[0].ds(RPM::TAG[:name]).DNEVR
    assert_equal 'P mainpkg-subpkg = 0:1.0-1.0',
      headers[1].ds(RPM::TAG[:name]).DNEVR
    assert_equal 'P mainpkg2 = 0:1.0-1.0',
      headers[2].ds(RPM::TAG[:name]).DNEVR
  end
end
