#!/usr/bin/env ruby

require "json"

module RPG
  class MapInfo; end
  class Map; end
  class AudioFile; end
  class Event
    class Page
      class Condition; end
      class Graphic; end
    end
  end
  class MoveRoute; end
  class MoveCommand; end
  class EventCommand; end
  class Tileset; end
end

class Color
  def self._load(_payload)
    allocate
  end
end

class Tone
  def self._load(_payload)
    allocate
  end
end

class Table
  attr_reader :xsize, :ysize, :zsize, :data

  def self._load(payload)
    table = allocate
    _dimensions, xsize, ysize, zsize, count = payload.unpack("V5")
    table.instance_variable_set(:@xsize, xsize)
    table.instance_variable_set(:@ysize, ysize)
    table.instance_variable_set(:@zsize, zsize)
    table.instance_variable_set(:@data, payload.byteslice(20, count * 2).unpack("v*"))
    table
  end
end

def ivar(object, name, fallback = nil)
  object.instance_variable_get("@#{name}") || fallback
end

def safe_text(value)
  text = value.to_s.dup
  text.force_encoding("UTF-8") if text.encoding == Encoding::ASCII_8BIT
  text.encode("UTF-8", invalid: :replace, undef: :replace, replace: "?")
end

essentials = File.expand_path(ARGV[0] || abort("usage: export_rmxp_map.rb ESSENTIALS_DIR MAP_ID"))
map_id = Integer(ARGV[1] || abort("missing MAP_ID"), 10)
data_dir = File.join(essentials, "Data")
map_path = File.join(data_dir, format("Map%03d.rxdata", map_id))
abort("map not found: #{map_path}") unless File.file?(map_path)

map = Marshal.load(File.binread(map_path))
tilesets = Marshal.load(File.binread(File.join(data_dir, "Tilesets.rxdata")))
map_infos = Marshal.load(File.binread(File.join(data_dir, "MapInfos.rxdata")))
tileset_id = ivar(map, :tileset_id, 0)
tileset = tilesets[tileset_id]
abort("tileset not found: #{tileset_id}") unless tileset

width = ivar(map, :width, 0)
height = ivar(map, :height, 0)
table = ivar(map, :data)
abort("invalid map table") unless table && table.xsize == width && table.ysize == height

layers = (0...table.zsize).map do |z|
  offset = z * width * height
  table.data.slice(offset, width * height)
end

events = ivar(map, :events, {}).values.map do |event|
  {
    "id" => ivar(event, :id, 0),
    "name" => safe_text(ivar(event, :name, "")),
    "x" => ivar(event, :x, 0),
    "y" => ivar(event, :y, 0)
  }
end

result = {
  "mapId" => map_id,
  "name" => safe_text(ivar(map_infos[map_id], :name, format("Map%03d", map_id))),
  "width" => width,
  "height" => height,
  "tilesetId" => tileset_id,
  "tilesetName" => safe_text(ivar(tileset, :tileset_name, "")),
  "autotileNames" => ivar(tileset, :autotile_names, []).map { |name| safe_text(name) },
  "layers" => layers,
  "passages" => ivar(tileset, :passages)&.data || [],
  "priorities" => ivar(tileset, :priorities)&.data || [],
  "terrainTags" => ivar(tileset, :terrain_tags)&.data || [],
  "events" => events
}

STDOUT.write(JSON.generate(result))
