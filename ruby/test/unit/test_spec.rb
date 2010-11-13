require 'test/unit'
require 'rpm'
require 'fileutils'

class TestSpec < Test::Unit::TestCase
  def setup
    @ts = RPM::Ts.new
    @fixture_path = File.expand_path(File.dirname(__FILE__) + '/../fixtures')
    @macros = [
      File.expand_path(File.dirname(__FILE__) + '/../../../macros/macros') ]

    RPM::Mc.init_macros(@macros.join ':')
    @spec = @ts.parse_spec(File.expand_path @fixture_path + '/mock.spec')
  end

  def test_get_sources
    src = @spec.sources
    
    assert src, 'Must return a sources array'
    assert src.length == 3, 'Must return all three defined sources'
    assert src == %w(barsource1 Foo-1.0 foosource1),
      'Must return the exactly defined source names'
  end

  def test_prep
    @spec.prep
  end
end
