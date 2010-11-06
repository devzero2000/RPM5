module RPM

  # The base class for RPM exceptions.
  class Error < Exception
    attr_reader :rc

    # Raise a new exception. Expects the message as the normal Exception
    # class, and additionally takes the rpmRC that lead to the exception.
    def initialize(rc = nil)
      @rc = rc
    end
  end
end
