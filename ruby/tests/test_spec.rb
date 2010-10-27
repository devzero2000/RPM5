require 'test/unit'
require 'rpm'

class TestSpec < Test::Unit::TestCase
  def setup
    @ts = RPM::Ts.new
  end

  def test_parse_spec
    s = @ts.parse_spec 'mock.spec'
    assert s, 'Must return a valid RPM::Spec object'
  end

  def test_get_sources
    s = @ts.parse_spec 'mock.spec'
    src = s.sources
    
    assert src, 'Must return a sources array'
    assert src.length == 3, 'Must return all three defined sources'
    puts src
    assert src == %w(barsource1 Foo-1.0 foosource1),
      'Must return the exactly defined source names'
  end
end
