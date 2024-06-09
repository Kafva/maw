#!/usr/bin/env ruby
require 'json'

def add_ccdb path
    return "" unless File.exist? path

    json = JSON.parse File.read path
    # Skip '[' and ']'
    JSON.dump(json).slice(1..-2)
end

ccdb_json = ""

# Add TU descriptions from the main project
Dir["build/.*.json"].each do |item|
    ccdb_json += File.read item
end

# Add TU descriptions from dependencies
ccdb_json += add_ccdb "deps/FFmpeg/compile_commands.json"
ccdb_json += add_ccdb "deps/libyaml/compile_commands.json"

puts '[' + ccdb_json + ']'
