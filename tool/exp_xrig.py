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


def vecAll(vec, val):
	for elem in vec:
		if elem != val: return False
	return True

class RigNode:
	def __init__(self, hnode, lvl):
		self.hnode = hnode
		self.lvl = lvl
		self.wmtx = hnode.worldTransform()
		self.lmtx = hnode.localTransform()
		self.path = hnode.path()
		self.name = hnode.name()
		self.type = hnode.type().name()
		self.attr = 0
		inp = hnode.inputConnectors()[0]
		if len(inp):
			self.parent = inp[0].inputNode()
		else:
			self.parent = None
		self.rordStr = xh.getRotOrd(self.hnode)
		self.xordStr = xh.getXformOrd(self.hnode)
		self.rord = xd.rotOrdFromStr(self.rordStr)
		self.xord = xd.xformOrdFromStr(self.xordStr)
		loc = self.lmtx.explode(self.xordStr, self.rordStr)
		self.lpos = loc["translate"]
		self.lrot = loc["rotate"]
		self.lscl = loc["scale"]

	def writeInfo(self, bw):
		bw.writeI16(self.selfIdx)
		bw.writeI16(self.parentIdx)
		bw.writeI16(self.nameId)
		bw.writeI16(self.pathId)
		bw.writeU16(self.typeId)
		bw.writeI16(self.lvl)
		bw.writeU16(self.attr)
		bw.writeU8(self.rord)
		bw.writeU8(self.xord)

class RigExporter(xd.BaseExporter):
	def __init__(self):
		xd.BaseExporter.__init__(self)
		self.sig = "XRIG"

	def findNode(self, nodeName):
		nodeId = -1
		if self.nodeMap and nodeName in self.nodeMap:
			nodeId = nodeMap[nodeName]
		return nodeId

	def build(self, rootPath):
		self.nodeMap = {}
		self.nodes = []
		self.buildSub(hou.node(rootPath), 0)
		self.maxLvl = 0
		self.defRot = True
		self.defScl = True
		for i, node in enumerate(self.nodes):
			node.selfIdx = i
			self.maxLvl = max(self.maxLvl, node.lvl)
			node.typeId = self.strLst.add(node.type)
			node.nameId = self.strLst.add(node.name)
			node.pathId = -1
			sep = node.path.rfind("/")
			if sep >= 0: node.pathId = self.strLst.add(node.path[:sep])
			if node.parent:
				node.parentIdx = self.nodeMap[node.parent.name()]
			else:
				node.parentIdx = -1
			if self.defRot:
				if not vecAll(node.lrot, 0.0):
					self.defRot = False
			if self.defScl:
				if not vecAll(node.lscl, 1.0):
					self.defScl = False

	def buildSub(self, hnode, lvl):
		self.nodeMap[hnode.name()] = len(self.nodes)
		self.nodes.append(RigNode(hnode, lvl))
		for link in hnode.outputConnectors()[0]:
			self.buildSub(link.outputNode(), lvl+1)

	def writeHead(self, bw, top):
		bw.writeU32(len(self.nodes)) # +20 nodeNum
		bw.writeU32(self.maxLvl + 1) # +24 lvlNum
		self.patchPos = bw.getPos()
		bw.writeU32(0) # +28 : +00 -> info[]
		bw.writeU32(0) # +2C : +04 -> wmtx[]
		bw.writeU32(0) # +30 : +08 -> imtx[]
		bw.writeU32(0) # +34 : +0C -> lmtx[]
		bw.writeU32(0) # +38 : +10 -> lpos[]
		bw.writeU32(0) # +3C : +14 -> lrot[]
		bw.writeU32(0) # +40 : +18 -> lscl[]
		bw.writeU32(0) # +44 : +1C -> reserved

	def writeData(self, bw, top):
		bw.align(0x10)
		bw.patch(self.patchPos, bw.getPos() - top)
		for node in self.nodes:
			node.writeInfo(bw)
		bw.align(0x10)
		bw.patch(self.patchPos + 4, bw.getPos() - top)
		for node in self.nodes:
			bw.writeFV(node.wmtx.asTuple())
		bw.patch(self.patchPos + 8, bw.getPos() - top)
		for node in self.nodes:
			bw.writeFV(node.wmtx.inverted().asTuple())
		bw.patch(self.patchPos + 0xC, bw.getPos() - top)
		for node in self.nodes:
			bw.writeFV(node.lmtx.asTuple())
		bw.patch(self.patchPos + 0x10, bw.getPos() - top)
		for node in self.nodes:
			bw.writeFV(node.lpos)
		if not self.defRot:
			bw.patch(self.patchPos + 0x14, bw.getPos() - top)
			for node in self.nodes:
				bw.writeFV(node.lrot)
		if not self.defScl:
			bw.patch(self.patchPos + 0x18, bw.getPos() - top)
			for node in self.nodes:
				bw.writeFV(node.lscl)

if __name__=="__main__":
	outPath = hou.expandString("$HIP/")
	outPath = exePath
	#outPath = r"D:/tmp/"
	outPath += "/_test.xrig"

	rootPath = "/obj/ANIM/root"
	rootPath = "/obj/root"

	rig = RigExporter()
	rig.build(rootPath)
	print "Saving rig to", outPath
	rig.save(outPath)
