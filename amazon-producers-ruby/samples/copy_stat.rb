#! /usr/bin/env ruby


require 'aws-sdk-core'
require 'multi_json'
require 'optparse'
require 'fileutils'
class CopyStat

  def initialize(sleep_between_puts, origin, dest)
    @sleep_between_puts = sleep_between_puts
    @origin = origin
    @dest = dest
  end

  def get_files
    a = Dir[@origin]
    if a == nil then
      return nil
    end
    return a.sort
  end

  def copy_files(a)
    a.each do |name|
      puts name
      FileUtils.mv name, @dest, :force => true
    end
  end
  def run(timeout=0)
    start = Time.now
    while (timeout == 0 || (Time.now - start) < timeout)
    a = get_files
    copy_files(a)
    sleep @sleep_between_puts
    end
  end
end

coper = CopyStat.new(120,"../../../stat/*.txt", "../../../mybucket")
coper.run(timeout=0)

