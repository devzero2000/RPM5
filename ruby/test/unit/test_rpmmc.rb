require 'test/unit'
require 'rpm'
require 'fileutils'

class TestRPMMC < Test::Unit::TestCase
  def setup
    @mc = RPM::Mc.new

    dirname = File.dirname __FILE__
    @macro_files = [ File.expand_path(dirname + '/../../../macros/macros'),
      File.expand_path(dirname + '/../fixtures/macros') ]
  end

  
  def test_add_del_expand
    @mc.add('foo Foo')
    @mc.add('foobar %{foo}Bar')

    assert_equal 'Foo', @mc.expand('%foo')
    assert_equal 'FooBar', @mc.expand('%foobar')

    @mc.del('foobar')
    assert_equal '%foobar', @mc.expand('%foobar')

    # Same for global MC
    
    RPM::Mc.add('foo Foo')
    RPM::Mc.add('foobar %{foo}Bar')

    assert_equal 'Foo', RPM::Mc.expand('%foo')
    assert_equal 'FooBar', RPM::Mc.expand('%foobar')

    RPM::Mc.del('foobar')
    assert_equal '%foobar', RPM::Mc.expand('%foobar')
  end


  def test_list
    @mc.add('foo Foo')
    @mc.add('foobar %{foo}Bar')
    @mc.add('bar(i) Argh %{-i}')
    assert_equal [["bar", "i", "Argh %{-i}"], ["foo", "", "Foo"],
      ["foobar", "", "%{foo}Bar"]], @mc.list

    RPM::Mc.add('foo Foo')
    RPM::Mc.add('foobar %{foo}Bar')
    RPM::Mc.add('bar(i) Argh %{-i}')
    assert RPM::Mc.list.include? ["bar", "i", "Argh %{-i}"]
  end


  def test_load_macro_file
    @mc.load_macro_file @macro_files[1], 0
    assert_equal 'Bar', @mc.expand('%foo')
    assert_equal 'FooBar', @mc.expand('%foobar')

    # Global MC
    RPM::Mc.load_macro_file @macro_files[1], 0
    assert_equal 'Bar', RPM::Mc.expand('%foo')
    assert_equal 'FooBar', RPM::Mc.expand('%foobar')
  end


  def test_init_macros
    @mc.init_macros(@macro_files.join ':')
    assert_equal 'FooBar', @mc.expand('%foobar')
    assert_equal '/usr', @mc.expand('%_usr')

    RPM::Mc.init_macros(@macro_files.join ':')
    assert_equal 'FooBar', RPM::Mc.expand('%foobar')
    assert_equal '/usr', RPM::Mc.expand('%_usr')
  end
end
