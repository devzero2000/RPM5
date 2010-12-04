require 'test/unit'
require 'rpm'
require 'fileutils'
require 'tempfile'

class TestSpec < Test::Unit::TestCase
  def setup
    @ts = RPM::Ts.new
    @fixture_path = File.expand_path File.dirname(__FILE__) + '/../fixtures'
    @tmpfile = Tempfile.new 'rpmtest'
    @macros = [
      File.expand_path(File.dirname(__FILE__) + '/../../../macros/macros'),
      File.expand_path(File.dirname(__FILE__) + 
          '/../../../macros/macros.rpmbuild') ]

    RPM::Mc.init_macros(@macros.join ':')
    RPM::Mc.add '_sourcedir ' + @fixture_path
    RPM::Mc.add '_builddir /tmp'
    RPM::Mc.add 'tmpfile ' + @tmpfile.path

    @spec = @ts.parse_spec File.expand_path(@fixture_path + '/mock.spec'),
      # rootURL, recursing
      '/', false,
      # passphrase, cookie, anyarch, force, verify
      '', nil, false, false, true
  end


  def teardown
    @tmpfile.close
    @tmpfile.unlink
  end


  def test_get_sources
    src = @spec.sources
    
    assert src, 'Must return a sources array'
    assert src.length == 3, 'Must return all three defined sources'
    assert src == %w(barsource1 Foo-1.0 foosource1.tar),
      'Must return the exactly defined source names'
  end

  def test_prep
    @spec.build 1, false

    assert File.exists?(@tmpfile), 'Testfile ' + @tmpfile.path + ' must exist'
    
    @tmpfile.rewind
    assert_equal 'prepared', @tmpfile.read.chomp,
      'Testfile must contain string from %prep section in mock.spec'
  end

  def test_build
    @spec.build 1<<0|1<<1, false

    assert File.exists?(@tmpfile), 'Testfile ' + @tmpfile.path + ' must exist'
    
    @tmpfile.rewind
    assert_equal 'built', @tmpfile.read.chomp,
      'Testfile must contain string from %build section in mock.spec'
  end

  def test_install
    @spec.build 1<<0|1<<2, false

    assert File.exists?(@tmpfile), 'Testfile ' + @tmpfile.path + ' must exist'
    
    @tmpfile.rewind
    assert_equal 'installed', @tmpfile.read.chomp,
      'Testfile must contain string from %install section in mock.spec'
  end

  def test_check
    @spec.build 1<<3, false

    assert File.exists?(@tmpfile), 'Testfile ' + @tmpfile.path + ' must exist'
    
    @tmpfile.rewind
    assert_equal 'checked', @tmpfile.read.chomp,
      'Testfile must contain string from %check section in mock.spec'
  end

  def test_clean
    @spec.build 1<<4, false
    assert File.exists?('/tmp/Foo-1.0'), 'Build directory' +
      ' must be removed by now.'
  end
end
