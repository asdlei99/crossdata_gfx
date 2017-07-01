# Author: Sergey Chaban <sergey.chaban@gmail.com>

import sys
import hou
import os
import imp
import re
import inspect
from math import *
from array import array

exePath = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename))

libName = "xd"
try: sys.modules[libName]
except:
	libFile, libFname, libDescr = imp.find_module(libName, [exePath])
	imp.load_module(libName, libFile, libFname, libDescr)
import xd

libName = "xh"
try: sys.modules[libName]
except:
	libFile, libFname, libDescr = imp.find_module(libName, [exePath])
	imp.load_module(libName, libFile, libFname, libDescr)
import xh


class NodeInfo:
	def __init__(self, node, nodePathId, nodeNameId, nodeTypeId):
		self.pathId = nodePathId
		self.nameId = nodeNameId
		self.typeId = nodeTypeId
		self.node = node
		self.rordStr = xh.getRotOrd(self.node)
		self.xordStr = xh.getXformOrd(self.node)
		self.rord = xd.rotOrdFromStr(self.rordStr)
		self.xord = xd.xformOrdFromStr(self.xordStr)

	def write(self, bw):
		bw.writeI16(self.pathId) # +00
		bw.writeI16(self.nameId) # +02
		bw.writeI16(self.typeId) # +04
		bw.writeU8(self.rord) # +06
		bw.writeU8(self.xord) # +07

class ClipFcvInfo:
	def __init__(self, param, nameInfo):
		self.param = param
		self.nameInfo = nameInfo

class KfrExporter(xd.BaseExporter):
	def __init__(self):
		xd.BaseExporter.__init__(self)
		self.sig = "XKFR"

	def build(self, chLst, minFrame, maxFrame, fps, nodeInfoFlg = True):
		self.fps = fps
		self.minFrame = int(minFrame)
		self.maxFrame = int(maxFrame)
		fcvLst = []
		for chPath in chLst:
			fcvLst.append(xh.FCurve(chPath, self.minFrame, self.maxFrame))
		self.fcv = []
		for fcv in fcvLst:
			if fcv.isAnimated():
				fcv.nodePathId = self.strLst.add(fcv.nodePath)
				fcv.nodeNameId = self.strLst.add(fcv.nodeName)
				fcv.chNameId = self.strLst.add(fcv.chName)
				self.fcv.append(fcv)
		self.nodeInfoLst = []
		if nodeInfoFlg:
			nodeMap = {}
			for fcv in self.fcv:
				node = fcv.prm.node()
				nodePath = node.path()
				if not nodePath in nodeMap:
					nodeId = len(self.nodeInfoLst)
					typeId = self.strLst.add(node.type().name())
					info = NodeInfo(node, fcv.nodePathId, fcv.nodeNameId, typeId)
					nodeMap[nodePath] = nodeId
					self.nodeInfoLst.append(info)

	def buildFromClip(self, clipPath, fps, nodeInfoFlg = True):
		self.fps = fps
		clip = hou.node(clipPath)
		if not clip: return
		self.nameId, self.pathId = self.strLst.addNameAndPath(clipPath)
		ntrk = clip.parm("numchannels").evalAsInt()
		self.minFrame = 0
		self.maxFrame = -1
		nodePath = "/obj"
		infoLst = []
		for i in xrange(ntrk):
			trkName = clip.parm("name"+str(i)).evalAsString()
			trkType = clip.parm("type"+str(i)).evalAsString()
			sep = trkName.rfind(":")
			nodeName = trkName[:sep]
			chanBaseName = trkName[sep+1:]
			sep = nodeName.rfind("/")
			if sep >= 0: nodeName = nodeName[sep+1:]
			trkSize = 0
			if trkType == "euler":
				trkSize = 3
			elif trkType == "float":
				trkSize = clip.parm("size"+str(i)).evalAsInt()
			for j in xrange(trkSize):
				chanName = chanBaseName
				if trkSize > 1: chanName += "xyzw"[j]
				param = clip.parm("value"+str(i)+("xyzw"[j]))
				nameInfo = xh.FcvNameInfo(chanName, nodeName, nodePath)
				infoLst.append(ClipFcvInfo(param, nameInfo))
				keys = param.keyframes()
				for n, k in enumerate(keys):
					fno = int(k.frame())
					if n:
						fnoMin = min(fno, fnoMin)
						fnoMax = max(fno, fnoMax)
					else:
						fnoMin = fno
						fnoMax = fno
				self.maxFrame = max(fnoMax, self.maxFrame)
		#print "Anim clip from", self.minFrame, "to", self.maxFrame
		fcvLst = []
		for i, info in enumerate(infoLst):
			#print i, info.nameInfo.nodeName, info.nameInfo.chName, info.param.path()
			fcvLst.append(xh.FCurve(info.param.path(), self.minFrame, self.maxFrame, info.nameInfo))
		self.fcv = []
		for fcv in fcvLst:
			if fcv.isAnimated():
				fcv.nodePathId = self.strLst.add(fcv.nodePath)
				fcv.nodeNameId = self.strLst.add(fcv.nodeName)
				fcv.chNameId = self.strLst.add(fcv.chName)
				self.fcv.append(fcv)
		self.nodeInfoLst = []
		if nodeInfoFlg:
			nodeMap = {}
			for fcv in self.fcv:
				nodePath = fcv.nodePath + "/" + fcv.nodeName
				if not nodePath in nodeMap:
					nodeId = len(self.nodeInfoLst)
					typeId = self.strLst.add("null")
					info = NodeInfo(hou.node(nodePath), fcv.nodePathId, fcv.nodeNameId, typeId)
					nodeMap[nodePath] = nodeId
					self.nodeInfoLst.append(info)

	def hasNodeInfo(self):
		return self.nodeInfoLst and len(self.nodeInfoLst) > 0

	def writeHead(self, bw, top):
		bw.writeF32(self.fps) # +20
		bw.writeI32(self.minFrame) # +24
		bw.writeI32(self.maxFrame) # +28
		bw.writeU32(len(self.fcv)) # +2C
		self.patchPos = bw.getPos()
		bw.writeU32(0) # +30 -> fcv[]
		bw.writeU32(len(self.nodeInfoLst)) # +34
		bw.writeU32(0) # +38 -> nodeInfo[]

	def writeData(self, bw, top):
		bw.align(0x10)
		bw.patch(self.patchPos, bw.getPos() - top)
		lstPatchTop = bw.getPos()
		for fcv in self.fcv:
			bw.writeI16(fcv.nodePathId) # +00
			bw.writeI16(fcv.nodeNameId) # +02
			bw.writeI16(fcv.chNameId) # +04
			bw.writeU16(fcv.getKeyNum()) # +06
			bw.writeF32(fcv.minVal) # +08
			bw.writeF32(fcv.maxVal) # +0C
			bw.writeU32(0) # +10 -> val
			bw.writeU32(0) # +14 -> lslope
			bw.writeU32(0) # +18 -> rslope
			bw.writeU32(0) # +1C -> fno
			bw.writeU32(0) # +20 -> func
			bw.writeI8(fcv.cmnFunc) # +24
			bw.writeU8(0)  # +25 reserved8
			bw.writeU16(0) # +26 reserved16
			bw.writeU32(0) # +28 reserved32_0
			bw.writeU32(0) # +2C reserved32_1
		if self.hasNodeInfo():
			bw.patch(self.patchPos + 8, bw.getPos() - top)
			for nodeInfo in self.nodeInfoLst:
				nodeInfo.write(bw)
		for i, fcv in enumerate(self.fcv):
			if not fcv.isConst():
				bw.patch(lstPatchTop + i*0x30 + 0x10, bw.getPos() - top)
				for val in fcv.vals:
					bw.writeF32(val)
				if not fcv.isBaked() and fcv.slopesFlg:
					bw.patch(lstPatchTop + i*0x30 + 0x14, bw.getPos() - top)
					for lslope in fcv.lslopes:
						bw.writeF32(lslope)
					bw.patch(lstPatchTop + i*0x30 + 0x18, bw.getPos() - top)
					for rslope in fcv.rslopes:
						bw.writeF32(rslope)
		maxFno = self.maxFrame - self.minFrame
		for i, fcv in enumerate(self.fcv):
			if not fcv.isConst() and not fcv.isBaked():
				bw.patch(lstPatchTop + i*0x30 + 0x1C, bw.getPos() - top)
				for fno in fcv.fnos:
					if maxFno < (1<<8): bw.writeU8(fno)
					elif maxFno < (1<<16): bw.writeU16(fno)
					else: bw.writeU32(fno)
		for i, fcv in enumerate(self.fcv):
			if not fcv.isSameFunc():
				bw.patch(lstPatchTop + i*0x30 + 0x20, bw.getPos() - top)
				for func in fcv.segFunc:
					bw.writeI8(func)

def test(outPath):
	minFrame = xh.getMinFrame()
	maxFrame = xh.getMaxFrame()
	chLst = xh.getChannelsInGroup("MOT") # "EXP"
	kfr = KfrExporter()
	kfr.build(chLst, minFrame, maxFrame, hou.fps())
	print "Saving keyframes to", outPath
	kfr.save(outPath)

def testClip(outPath):
	kfr = KfrExporter()
	kfr.buildFromClip("/obj/motionfx/MOT_newclip1", hou.fps())
	print "Saving clip keyframes to", outPath
	kfr.save(outPath)

if __name__=="__main__":
	outPath = hou.expandString("$HIP/")
	outPath = exePath
	#outPath = r"D:/tmp/"
	outPath += "/_test.xkfr"

	test(outPath)
	#testClip(outPath)
