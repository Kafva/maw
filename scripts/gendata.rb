#!/usr/bin/env ruby
require 'open3'
require 'json'
require 'tempfile'
require 'yaml'
require 'fileutils'
require 'optparse'

class CommandError < StandardError
    # @return [String]
    attr_reader :program
    # @return [String]
    attr_reader :out

    # @param program [String]
    # @param out [String]
    def initialize program, out
        @program = program
        @out = out
        super("Command error '#{program}': #{out}")
    end
end

def die msg
    err msg
    exit 1
end

# @param args [Array]
def err args
    warn "\e[91m*\e[0m #{args}"
end

# @param args [Array]
def info args
    warn "\e[92m*\e[0m #{args}"
end

# @param args [Array]
def debug args
    return unless FLAGS[:debug]

    warn "\e[94m*\e[0m #{args}"
end

# @param program [String]
# @param args [Array]
# @return [Process::Status,String]
def system_run program, args
    cmdarr = ([program] + args)
    debug cmdarr.join ' '
    stdout, _, status = Open3.capture3(*cmdarr)
    die "Command failed: #{cmdarr.join ' '}" unless status.success?
    [status, stdout]
rescue Interrupt
    die "Cancelled"
rescue CommandError => e
    err e.out
    die "Command failed: #{e.program}"
end

# Create random unicode string
# https://en.wikipedia.org/wiki/Unicode_block
# @param minlen [Array]
# @param maxlen [Array]
def randstr minlen, maxlen
    str_len = rand minlen..maxlen
    out = ''
    (0..str_len).each do |_|
        choice = rand(0..10) % 10
        if choice.zero? # space
            out += ' '
        elsif choice < 5 # ascii
            out += (rand CHAR_RANGES[0]).chr 'utf-8'
        else # any character set
            char_range = CHAR_RANGES.sample
            out += (rand char_range).chr 'utf-8'
        end
    end
    out.gsub(%r{/}, '').gsub('\\', '\\\\')
end

def time_taken
    start_time = Time.now
    yield
    end_time = Time.now
    elapsed_time = end_time - start_time
    info "Done: #{elapsed_time.round(2)} seconds"
end

# @param outputfile [String]
# @param title [String, void]
# @param album [String, void]
# @param artist [String, void]
# @param color [String, void]
# @param duration [Integer, void]
def generate_video(outputfile,
                   title: nil,
                   album: nil,
                   artist: nil,
                   color: nil,
                   duration: 30,
                   random_metadata: true)
    res = "1280x720"
    suffix = File.extname outputfile
    tmpvideo = Tempfile.new ["maw", suffix]
    # Create a video with alternating colors
    system_run "ffmpeg", ["-y"] +
                         ["-f", "lavfi", "-i", "color=c=#{color}:s=#{res}:d=5"] +
                         ["-f", "lavfi", "-i", "color=c=black:s=#{res}:d=5"] +
                         ["-filter_complex", "[0:v][1:v]concat=n=2:v=1:a=0"] +
                         [tmpvideo.path]

    # Combine it with a silent audio stream and metadata
    system_run "ffmpeg", ["-y"] +
                         ["-f", "lavfi", "-i", "anullsrc=duration=#{duration}"] +
                         ["-i", tmpvideo.path] +
                         # Metadata
                         generate_metadata(title: title,
                                           album: album,
                                           artist: artist,
                                           random: random_metadata) +
                         [outputfile]
ensure
    tmpvideo&.unlink
end

# @param outputfile [String]
# @param title [String, void]
# @param album [String, void]
# @param artist [String, void]
# @param cover_color [String, void]
# @param cover_res [String, void]
# @param duration [Integer, void]
def generate_audio(outputfile,
                   title: nil,
                   album: nil,
                   artist: nil,
                   cover_color: nil,
                   cover_res: "1280x720",
                   duration: 30,
                   random_metadata: true)
    system_run "ffmpeg", ["-y"] +
                         # Audio source
                         ["-f", "lavfi", "-i", "anullsrc=duration=#{duration}"] +
                         # Image source
                         (cover_color.nil? ? [] : ["-f", "lavfi",  "-i", "color=c=#{cover_color}:s=#{cover_res}"]) +
                         # Audio output
                         ["-map", "0", "-c:a", "aac", "-shortest"] +
                         # Image output
                         (cover_color.nil? ? [] :
                           ["-map", "1", "-frames:v", "1", "-c:v", "png", "-disposition:1", "attached_pic"]) +
                         # Metadata
                         generate_metadata(title: title,
                                           album: album,
                                           artist: artist,
                                           random: random_metadata) +
                         [outputfile]
end

def generate_dual_audio outputfile
    system_run "ffmpeg", ["-y",
               "-f", "lavfi", "-i", "anullsrc=duration=30",
               "-f", "lavfi", "-i", "anullsrc=duration=30",
               "-map", "0", "-c:a", "aac", "-map", "1", "-c:a", "aac", outputfile]
end

def generate_dual_video outputfile
    tmpcover = Tempfile.new ["maw", ".png"]
    generate_cover "yellow", tmpcover.path
    system_run "ffmpeg", ["-y"] +
                         # Audio source
                         ["-f", "lavfi", "-i", "anullsrc=duration=30"] +
                         # Image sources
                         ["-i", tmpcover.path] +
                         ["-i", tmpcover.path] +
                         # Audio output
                         ["-map", "0", "-c:a", "aac", "-shortest"] +
                         # Image outputs
                         ["-map", "1", "-c:v", "copy", "-disposition:1", "attached_pic"] +
                         ["-map", "2", "-c:v", "copy", "-disposition:1", "attached_pic"] +
                         [outputfile]
ensure
    tmpcover&.unlink
end

# @param title [String, void]
# @param album [String, void]
# @param artist [String, void]
def generate_metadata(title: nil,
                      album: nil,
                      artist: nil,
                      random: true)
    maxlen = 12
    maxlen_text = 32
    ["-metadata", "title=#{title.nil? ? randstr(1, maxlen) : title}",
    "-metadata", "album=#{album.nil? ? randstr(1, maxlen) : album}",
    "-metadata", "artist=#{artist.nil? ? randstr(1, maxlen) : artist}",
    "-metadata", "comment=#{random ? randstr(1, maxlen_text) : "comment"}",
    "-metadata", "description=#{random ? randstr(1, maxlen_text) : "description"}",
    "-metadata", "genre=#{random ? randstr(1, maxlen) : "genre"}",
    "-metadata", "composer=#{random ? randstr(1, maxlen) : "composer"}",
    "-metadata", "copyright=#{random ? randstr(1, maxlen) : "copyright"}",
    "-metadata", "synopsis=#{random ? randstr(1, maxlen_text) : "synopsis"}"]
end

def generate_cover color, outputfile
    system_run "convert", ["-size", "1280x720", "xc:#{color}", outputfile]
end

def setup
    cfg_yaml = <<~HEREDOC
        art_dir: #{ART_ROOT}
        music_dir: #{MUSIC_ROOT}
        playlists:
            first:
              - red/red1.mp4
              - red/red2.mp4
            second:
              - blue/blue1.m4a
              - blue/blue2.m4a
        metadata:
            red:
              album: Red album
              artist: Red artist
              clear_non_core_fields: true
            blue:
              album: Blue album
              artist: Blue artist
              cover: blue.png
              clear_non_core_fields: true
    HEREDOC

    FileUtils.rm_rf TOP
    FileUtils.mkdir_p ART_ROOT
    FileUtils.mkdir_p MUSIC_ROOT
    FileUtils.mkdir_p "#{TOP}/unit"
    File.write(CFG, cfg_yaml)


    # Unit test data
    generate_cover '#00d7d7', "#{ART_ROOT}/blue-1.png"
    generate_dual_audio "#{TOP}/unit/dual_audio.mp4"
    generate_dual_video "#{TOP}/unit/dual_video.mp4"
    generate_audio "#{TOP}/unit/only_audio.m4a"

    generate_audio "#{TOP}/unit/add_cover.m4a"
    generate_audio "#{TOP}/unit/clear_cover.m4a"
    generate_audio "#{TOP}/unit/crop_cover.m4a",
                   cover_color: "#98fb98",
                   cover_res: "1280x720"
    generate_audio "#{TOP}/unit/replace_cover.m4a",
                   cover_color: "#00ff00"
    generate_audio "#{TOP}/unit/keep_all.m4a",
                   title: "keep_all",
                   artist: "Artist",
                   album: "Album",
                   cover_color: "#5f1eb0"
    generate_audio "#{TOP}/unit/clear_non_core_fields.m4a",
                   title: "clear_non_core_fields",
                   artist: "Artist",
                   album: "Album",
                   cover_color: "white",
                   random_metadata: false

    # E2E testing data
    ALBUMS.each do |album|
        FileUtils.mkdir_p "#{MUSIC_ROOT}/#{album}"
        generate_cover album, "#{ART_ROOT}/#{album}.png"
        basepath = "#{MUSIC_ROOT}/#{album}"

        (0...1).each do |i|
            # Audio stream with cover
            generate_audio "#{basepath}/audio_#{album}_#{i}.m4a",
                           cover_color: album
        end
        (0...3).each do |i|
            FileUtils.cp "#{basepath}/audio_#{album}_#{i}.m4a",
                         "#{basepath}/audio_#{album}_#{i + 1}.m4a"
        end
    end

    system "tree", "--noreport", "#{TOP}/music" if FLAGS[:debug]
end

CHAR_RANGES = [
    (0x20..0x7f), # ascii
    (0x80..0x2af), # extended latin1
    (0x3040..0x309f), # hiragana
    (0x1f600..0x1f64f) # emoticons
].freeze
FLAGS = { # rubocop:disable Style/MutableConstant
    debug: false
}

ALBUMS = ["blue", "red"].freeze
TOP = "#{File.dirname(__FILE__)}/../.testenv".freeze
ART_ROOT = "#{TOP}/art".freeze
MUSIC_ROOT = "#{TOP}/albums".freeze
CFG = "#{TOP}/maw.yml".freeze

parser = OptionParser.new do |opts|
    opts.banner = "usage: #{File.basename $0} [FLAGS]"
    opts.on('-d', '--debug', 'Show debug information') do |_|
        FLAGS[:debug] = true
    end
    opts.on('-h', '--help', 'Show help and exit') do |_|
        opts.display
        exit
    end
end

begin
    parser.parse!
rescue StandardError => e
    die e.message, parser.help
end

time_taken do
    info "Setting up testdata..."
    setup
    status, out = system_run "tree", ["--noreport", TOP]
    puts out if status.success?
end
