#!/Applications/Kicad/kicad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
# -*- coding: utf-8 -*-
import os
import sys
from math import *
import cmath
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('-p', "--path", action='store', required=True, help="Path to kicad_pcb file")
parser.add_argument('--delete-all-traces', action='store_true', help='Delete All Traces in the pcbnew and then exit')
parser.add_argument('--delete-all-drawings', action='store_true', help='Delete all lines & texts in the pcbnew and then exit')
parser.add_argument('--delete-short-traces', action='store_true', help='Delete traces of zero or very small length and exit')
parser.add_argument('--dry-run', action='store_true', help='Don\'t save results')
parser.add_argument('-v', dest='verbose', action='count', help="Verbose")
parser.add_argument('--skip-traces', action='store_true', help='Don\'t add traces')
parser.add_argument('--hide-pixel-labels', action='store_true', help="For all modules like D*, set reference text to hidden")
args = parser.parse_args()

sys.path.insert(0, "/Applications/Kicad/kicad.app/Contents/Frameworks/python/site-packages/")
sys.path.append("/Library/Python/3.7/site-packages")
sys.path.append("/Library/Frameworks/Python.framework/Versions/3.7/lib/python3.7/site-packages")

import kicad
import pcbnew

from kicad.pcbnew import Board
from kicad.util.point import Point2D

def sign(x):
  return 1 if x >= 0 else -1

def circle_pt(theta, radius):
  return Point(radius*cos(theta), radius*sin(theta))

class Point(object):
  @classmethod
  def fromWxPoint(cls, wxPoint):
    return Point(wxPoint.x / pcbnew.IU_PER_MM, wxPoint.y / pcbnew.IU_PER_MM)

  @classmethod
  def fromComplex(cls, c):
    return Point(c.real, c.imag)

  def __init__(self, x, y):
    self.x = x
    self.y = y

  def wxPoint(self):
    return pcbnew.wxPoint(self.x * pcbnew.IU_PER_MM , self.y * pcbnew.IU_PER_MM)
  
  def point2d(self):
    return Point2D(x,y)

  def translate(self, vec):
    self.x += vec.x
    self.y += vec.y

  def translated(self, vec):
    return Point(self.x+vec.x, self.y+vec.y)

  def polar_translated(self, distance, angle):
    vec = distance * cmath.exp(angle * 1j)
    return self.translated(Point.fromComplex(vec))


  def __getitem__(self, i):
    if i == 0:
      return self.x
    if i == 1:
      return self.y
    raise IndexError("index out of range")

  def distance_to(self, other):
    return sqrt((self.x - other.x) ** 2 + (self.y - other.y) ** 2)

  def __str__(self):
    return "(%0.2f, %0.2f)" % (self.x, self.y)

  def __add__(self, oth):
    return Point(self.x + oth.x, self.y + oth.y)

  def __eq__(self, oth):
    return self.x == oth.x and self.y == oth.y


###############################################################################

  
class KiCadPCB(object):
  def __init__(self, path):
    self.pcb_path = os.path.abspath(path)
    self.board = Board.from_file(self.pcb_path)

    self.numlayers = pcbnew.PCB_LAYER_ID_COUNT

    self.layertable = {}
    for i in range(self.numlayers):
      self.layertable[self.board._obj.GetLayerName(i)] = i

    self.netcodes = self.board._obj.GetNetsByNetcode()
    self.netnames = {}  
    for netcode, net in self.netcodes.items():
      self.netnames[net.GetNetname()] = net

  def save(self):
    print("Saving...")
    backup_path = self.pcb_path + ".layoutbak"
    if os.path.exists(backup_path):
      os.unlink(backup_path)
    os.rename(self.pcb_path, backup_path)
    print("  Backing up '%s' to '%s'..." % (self.pcb_path, backup_path))
    assert self.board._obj.Save(self.pcb_path)
    print("Saved!")

  def deleteEdgeCuts(self):
    # delete old edge cuts
    for drawing in self.board._obj.Drawings():
      if drawing.IsOnLayer(self.layertable['Edge.Cuts']):
        self.board._obj.Delete(drawing)
        print("Deleting edge cut drawing")

  def deleteAllTraces(self):
    tracks = self.board._obj.GetTracks()
    for t in tracks:
      print("Deleting track {}".format(t))
      self.board._obj.Delete(t)
  
  def deleteAllDrawings(self):
    while list(self.board._obj.GetDrawings()):
        drawing = list(self.board._obj.GetDrawings())[0]
        print("Deleting drawing {}".format(drawing))
        self.board._obj.Delete(drawing)
  def deleteShortTraces(self):
    print("FIXME: disabled because it removes vias")
    exit(1)
    tracks = board._obj.GetTracks()
    for t in tracks:
      start = t.GetStart()
      end = t.GetEnd()
      length = sqrt((end.x - start.x)**2 + (end.y - start.y)**2)
      if length < 100: # millionths of an inch
        print("Deleting trace of short length {}in/1000000".format(length))
        board._obj.Delete(t)
      elif args.hide_pixel_labels:
        hid = 0
        for m in board._obj.m_Modules:
          if m.GetReference().startswith('D'):
            if m.Reference().IsVisible():
              m.Reference().SetVisible(False)
              hid+=1
        print("Hid %i reference labels" % hid)

  def clear_tracks_for_module(self, module): 
    for pad in module._obj.Pads():
      pad_position = Point.fromWxPoint(pad.GetPosition())
      tracks = self.board._obj.GetTracks()
      for track in tracks:
        track_start = Point.fromWxPoint(track.GetStart())
        track_end = Point.fromWxPoint(track.GetEnd())
        track_net = track.GetNet()
        thresh = .0001
        d1 = pad_position.distance_to(track_start)
        d2 = pad_position.distance_to(track_end)

        if d1 < thresh or d2 < thresh:

          print("Deleting old track for module %s pad %s net %s" % (module.reference, pad.GetPadName(), track_net.GetNetname()))
          self.board._obj.Delete(track)

          vias = []
          vias.append(self.board._obj.GetViaByPosition(track_start.wxPoint()))
          vias.append(self.board._obj.GetViaByPosition(track_end.wxPoint()))


          # FIXME: Deleting a via crashes inside kicad API
          # should be able to find and delete zero-length traces to get these??

          # print("vias matching track start: %s" % str())
          # print("vias matching track end: %s" % str(board._obj.GetViaByPosition(track_end.wxPoint())))
          # for via in vias:
          #  if via:
          # for via in board.vias:
          #   if track_start.distance_to(via.position) < thresh or track_end.distance_to(via.position) < thresh:
          #    print("Deleting old via connected to track on net %s" % (track_net.GetNetname(), ))
          #    board._obj.Delete(via)

  # Drawing Tools
  def draw_segment(self, start, end, layer='F.Silkscreen', width=0.15):
    line = pcbnew.PCB_SHAPE()
    self.board._obj.Add(line)
    line.SetShape(pcbnew.S_SEGMENT)
    line.SetStart(start.wxPoint())
    line.SetEnd(end.wxPoint())
    print("LINE from (%f, %f) to (%f, %f)" % (start.x, start.y, end.x, end.y))
    line.SetLayer(self.layertable[layer])
    line.SetWidth(int(width * pcbnew.IU_PER_MM))
    return line
  
  def draw_circle(self, center, radius, layer='F.Silkscreen', width=0.15):
      circle = pcbnew.PCB_SHAPE()
      self.board._obj.Add(circle)
      circle.SetShape(pcbnew.SHAPE_T_CIRCLE)
      circle.SetCenter(center.wxPoint())
      circle.SetArcAngleAndEnd(360)
      circle.SetEnd((center + Point(radius,0)).wxPoint()) # Sets radius.. somehow
      circle.SetLayer(self.layertable[layer])
      circle.SetWidth(int(width * pcbnew.IU_PER_MM))
      return circle
  
  # def draw_arc_1(self, center, radius, start_angle, stop_angle,
  #                 layer='F.Silkscreen', width=0.15):
      
  #     print("Drawing arc with center at (%f,%f), radius: %f, angle %f -> %f" % (center.x, center.y, radius, start_angle, stop_angle))
  #     arcStartPoint = radius * cmath.exp(start_angle * 1j)
  #     arcStartPoint = center.translated(Point.fromComplex(arcStartPoint))
  #     print("arcStartPoint %s" % str(arcStartPoint));
  #     angle = stop_angle - start_angle
  #     arc = pcbnew.PCB_ARC(self.board._obj)
  #     self.board._obj.Add(arc)
  #     arc.SetShape(pcbnew.S_ARC)
  #     arc.SetCenter(center.wxPoint())
  #     arc.SetArcStart(arcStartPoint.wxPoint())
  #     arc.SetAngle(angle * 180/pi * 10)
  #     arc.SetLayer(self.layertable[layer])
  #     arc.SetWidth(int(width * pcbnew.IU_PER_MM))
  #     return arc

  # def draw_arc_2(self, center, arcStartPoint, totalAngle,
  #                 layer='F.Silkscreen', width=0.15):
  #     print("Drawing arc with center: (%f, %f), arc point at (%f,%f), total angle %f" % (center.x, center.y, arcStartPoint.x, arcStartPoint.y, totalAngle))
  #     arc = pcbnew.PCB_ARC(self.board._obj)
  #     self.board._obj.Add(arc)
  #     arc.SetShape(pcbnew.S_ARC)
  #     arc.SetCenter(center.wxPoint())
  #     arc.SetArcStart(arcStartPoint.wxPoint())
  #     arc.SetAngle(totalAngle * 180/pi * 10)
  #     arc.SetLayer(self.layertable[layer])
  #     arc.SetWidth(int(width * pcbnew.IU_PER_MM))
  #     return arc

  def add_copper_trace(self, start, end, net):
    track = pcbnew.PCB_TRACK(self.board._obj)
    track.SetStart(start)
    track.SetEnd(end)
    track.SetLayer(self.layertable["F.Cu"])
    track.SetWidth(int(.25 * 10**6))
    track.SetNet(net)
    self.board._obj.Add(track)
    print("adding track from {} to {} on net {}".format(start, end, net.GetNetname()))
    return track

  def add_via(self, position, net):
    via = pcbnew.PCB_VIA(self.board._obj)
    via.SetPosition(position)
    via.SetLayerPair(self.layertable["F.Cu"], self.layertable["B.Cu"])
    via.SetDrill(int(0.254 * pcbnew.IU_PER_MM))
    via.SetWidth(int(0.4064 * pcbnew.IU_PER_MM))
    via.SetNet(net)
    print("Adding via on net %s at %s" % (net.GetNetname(), str(position)))
    self.board._obj.Add(via)


###############################################################################


class PCBLayout(object):
  center = Point(100,100)
  edge_cut_line_thickness = 0.05
  pixel_pad_map = {"2": "5", "3": "4"} # connect SCK and SD* for APA102c-2020
  
  ####

  def __init__(self, path):
    self.kicadpcb = KiCadPCB(path)
    self.board = self.kicadpcb.board

  prev_series_pixel = None
  pixel_prototype = None
  series_pixel_count = 1
  placed_pixel_points = []

  def drawSegment(self, start, end, layer='F.Silkscreen', width=0.15):
    if type(start) is tuple:
      start = Point(start[0], start[1])
    if type(end) is tuple:
      end = Point(start[0], start[1])
    self.kicadpcb.draw_segment(self.center+start, self.center+end, layer, width)

  def seriesPixelDiscontinuity(self):
    self.prev_series_pixel = None

  def placeSeriesPixel(self, point, orientation, allowOverlaps=True):
    reference = "D%i" % self.series_pixel_count
    self.series_pixel_count += 1
    
    if self.pixel_prototype is None:
      print("Loading pixel footprint...")
      self.pixel_prototype = pcbnew.FootprintLoad("/Library/Application Support/kicad/modules/LED_SMD.pretty", "LED-APA102-2020")
      self.pixel_prototype.SetLayer(self.kicadpcb.layertable["F.Cu"])

    # if self.prev_series_pixel is not None:
    #   self.drawSegment(Point.fromWxPoint(self.prev_series_pixel.GetPosition()), point, width = 0.1)

    if not allowOverlaps:
      overlapThresh = 0.8
      for placed in self.placed_pixel_points:
        if (self.center + point).distance_to(placed) < overlapThresh:
          print("Skipping pixel %s at point %s due to overlap" % (reference, point))
          return
      self.placed_pixel_points.append(self.center+point)

    print("Placing pixel %s at point %s (relative center %s) orientation %f" % (reference, point, self.center, orientation));
    pixel = pcbnew.FOOTPRINT(self.pixel_prototype)
    pixel.SetPosition((self.center + point).wxPoint())
    
    pixel.SetReference(reference)
    # module.position = (point + self.center).point2d()
    kicad_orientation = orientation * 180/pi * 10 # kicad stores 10th's of a degree?? *shrug*
    pixel.SetOrientation(kicad_orientation)
    pixel.Reference().SetVisible(False)
    self.kicadpcb.board._obj.Add(pixel)

    for pad in pixel.Pads():
      # FIXME: Remove the center pads? esp if soldering by hand?
      # fortunately the corner GND pad always seems to come first in the list
      
      if not args.skip_traces and pad.GetPadName() == '6': # GND
        # draw trace outward from ground
        end = Point.fromWxPoint(pad.GetPosition()).polar_translated(0.8, -pi/2-orientation)
        self.kicadpcb.add_copper_trace(pad.GetPosition(), end.wxPoint(), pad.GetNet())

        # add a via
        self.kicadpcb.add_via(end.wxPoint(), pad.GetNet())

        break

    # Add tracks from the previous pixel, connecting pad 5 to pad 2 and pad 4 to pad 3
    if not args.skip_traces and self.prev_series_pixel is not None:
      for prev_pad in self.prev_series_pixel.Pads():
        if prev_pad.GetPadName() in self.pixel_pad_map:
          tracks = self.kicadpcb.board._obj.TracksInNet(prev_pad.GetNet().GetNetCode())
          if tracks:
            # skip pad, already has traces
            if args.verbose:
              print("Skipping pad, already has tracks: {}".format(prev_pad))
            continue
          # for net in board._obj.TracksInNet(prev_pad.GetNet().GetNet()):
          #   board._obj.Delete(t)

          # then connect the two pads
          for pad in pixel.Pads():
            if pad.GetPadName() == self.pixel_pad_map[prev_pad.GetPadName()]:
              start = prev_pad.GetPosition()
              end = pad.GetPosition()
              print("Adding track from pixel {} pad {} to pixel {} pad {}".format(self.prev_series_pixel.reference, prev_pad.GetPadName(), pixel.reference, pad.GetPadName()))
              self.kicadpcb.add_copper_trace(start, end, pad.GetNet())

           # FIXME: Draw GND and +5V traces?

    self.prev_series_pixel = pixel

  def doLayout(self, args):
    if args.delete_all_traces:
      self.kicadpcb.deleteAllTraces()
    if args.delete_all_drawings:
      self.kicadpcb.deleteAllDrawings()
    elif args.delete_short_traces:
      self.kicadpcb.deleteShortTraces()
    else:
      self.drawEdgeCuts()
      self.placePixels()

    if not args.dry_run:
      self.kicadpcb.save()
  
  ### to override
  def drawEdgeCuts(self):
    self.kicadpcb.deleteEdgeCuts()
    pass
  def placePixels(self):
    import re
    for fp in self.kicadpcb.board._obj.GetFootprints():
      if re.match("^D\d+$",fp.GetReference()):
        print("Removing old footprint for pixel %s" % fp.GetReference())
        self.kicadpcb.board._obj.Delete(fp)


###############################################################################


class LayoutHelixLoop(PCBLayout):
  edge_radius = 72#mm
  pixelSpacing = 3.922
  helixRadius = 56.6
  helixAmplitude = 12
  helixCycles = 6
  pixelRotation = 0.98

  # edge_radius = 68#mm
  # pixelSpacing = 3.849
  # helixRadius = 55
  # helixAmplitude = 7.15
  # helixCycles = 6
  # pixelRotation = 0.68

  # edge_radius = 64#mm
  # pixelSpacing = 3.806
  # helixRadius = 51
  # helixAmplitude = 8
  # helixCycles = 7

  def drawEdgeCuts(self):
    super().drawEdgeCuts()
    self.kicadpcb.draw_circle(self.center, self.edge_radius, layer='Edge.Cuts', width=self.edge_cut_line_thickness)
    # FIXME: board edge follows shape
  
  def placePixelWave(self,phase):
    
    segments = 80000
    phase /= self.helixCycles
    last_placement = Point(inf, inf)
    first_placement = None

    print("Placing Pixel Wave at phase", phase)
    for s in range(0,segments):
      theta = s*2*pi/segments
      radius = self.helixRadius + self.helixAmplitude * sin(theta * self.helixCycles)
      pos = Point(radius * sin(theta+phase), radius * cos(theta+phase))

      # space the pixels roughly evenly along the curve
      if last_placement.distance_to(pos) < self.pixelSpacing:
        continue
      # skip overlap at the endpoints
      if first_placement is not None:
        if first_placement.distance_to(pos) < self.pixelSpacing*0.9:
          continue
      else:
        first_placement = pos
      
      orientation = theta + phase - cos(theta * self.helixCycles) * self.pixelRotation
      self.placeSeriesPixel(pos, orientation, allowOverlaps=False)

      last_placement = pos
    self.seriesPixelDiscontinuity()

    # lineLength = 7
    # for loopy in range(0, self.helixCycles):
    #   theta = 3*pi/2/self.helixCycles + loopy * 2 * pi / self.helixCycles #wave min
    #   orientation = pi/2+theta + phase

    #   # inward lines
    #   if phase == 0:
    #     for px in range(lineLength):
    #       radius = self.helixRadius - self.helixAmplitude - self.pixelSpacing * (px+1)
    #       pos = Point(radius * sin(theta+phase), radius * cos(theta+phase))
    #       if px == lineLength-1: # last px is in the circle
    #         orientation -= pi/2
    #       self.placeSeriesPixel(pos, orientation, allowOverlaps=False)
  

  def placePixels(self):
    super().placePixels()

    # # remove previous tracks
    # modules = dict(zip((m.reference for m in self.kicadpcb.board.modules), (m for m in self.kicadpcb.board.modules)))

    # for module in modules.values():
    # 	if module.reference.startswith('D'):
    # 		clear_tracks_for_module(module)
    self.placePixelWave(0)
    self.placePixelWave(pi)

    for i in range(3):
      print("Placing Spiral", i)
      spiralStartRadius = 0.45 * self.helixRadius
      spiralStartTheta = i * 2*pi/3 + 7/self.helixCycles/2
      spiralCenter = circle_pt(spiralStartTheta, spiralStartRadius)
      spiralRadiusPerLoop = 10.4

      # last_point = None
      last_placement = Point(inf, inf)
      segments = 40000
      
      loops = 2.04

      linear_adjust_start = 3.7 * pi
      for s in range(segments):
        radius = 2 + loops * spiralRadiusPerLoop * s / segments
        spiralTheta = 2*pi * loops * s / segments - 0.022
        theta = spiralTheta - spiralStartTheta
        orientation = theta-0.1
        if spiralTheta > linear_adjust_start:
          radius += 4*(spiralTheta -linear_adjust_start)
          orientation -= 0.12
          pass
        
        pos = spiralCenter + Point(radius*sin(theta), radius*cos(theta))
        
        # if last_point is not None:
        #   self.drawSegment(last_point, pos, width = 0.01)
        # last_point = pos
        if last_placement.distance_to(pos) < self.pixelSpacing:
          continue
        # self.drawSegment(spiralCenter, pos)
        self.placeSeriesPixel(pos, orientation, allowOverlaps=True)
        
        last_placement = pos
      self.seriesPixelDiscontinuity()

    # # inner circle
    # circleCount = 18
    # for i in range(0,circleCount):
    #   radius = 10
    #   theta = 2*pi * i / circleCount
    #   orientation = theta
    #   pos = Point(radius * sin(theta), radius * cos(theta))
    #   self.placeSeriesPixel(pos, orientation, allowOverlaps=True)


    # guides = 24
    # for i in range(guides):
    #   theta = i * 2 * pi/guides
    #   self.drawSegment((0,0), circle_pt(theta,self.edge_radius))
    

layout = LayoutHelixLoop(args.path)    

if __name__ == '__main__':
  layout.doLayout(args)

