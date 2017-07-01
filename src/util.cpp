#include "crossdata.hpp"
#include "gex.hpp"

#include "util.hpp"

class cExprCtx : public sxCompiledExpression::ExecIfc {
protected:
	sxCompiledExpression::Stack mStk;

public:
	float mRes;
	float mRoughness;
public:
	cExprCtx() {}

	void init() {
		mStk.alloc(128);
		mRes = 0.0f;
		mRoughness = 0.0f;
	}
	void reset() {
		mStk.free();
	}

	sxCompiledExpression::Stack* get_stack() { return &mStk; }
	void set_result(float val) { mRes = val; }
	float ch(const sxCompiledExpression::String& path) {
		float res = 0.0f;
		if (path.is_valid()) {
			if (nxCore::str_eq("rough", path.mpChars)) {
				res = mRoughness;
			}
		}
		return res;
	}
};

static struct _WK {
	sxExprLibData* pLib;
	cExprCtx ctx;
	int eidRoughCvt;

	void clear() {
		pLib = nullptr;
		eidRoughCvt = -1;
	}
} WK;

void util_init() {
	WK.clear();
	const char* pLibPath = DATA_PATH("exprlib.xcel");
	sxData* pData = nxData::load(pLibPath);
	if (!pData) return;
	WK.pLib = pData->as<sxExprLibData>();
	if (!WK.pLib) return;
	WK.ctx.init();
	WK.eidRoughCvt = WK.pLib->find_expr_idx("EXPRLIB", "roughCvt");
}

void util_reset() {
	WK.ctx.reset();
	nxData::unload(WK.pLib);
	WK.clear();
}

void MOTION_LIB::init(const char* pBasePath, const sxRigData& rig) {
	bool dbgInfo = true;
	if (!pBasePath) return;
	char path[XD_MAX_PATH];
	XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/motlib.fcat", pBasePath);
	sxData* pData = nxData::load(path);
	if (!pData) return;
	mpCat = pData->as<sxFileCatalogue>();
	if (!mpCat) return;
	int nmot = get_motions_num();
	if (nmot <= 0) return;
	if (dbgInfo) nxCore::dbg_msg("Motion Lib @ %s\n", pBasePath);
	mppKfrs = (sxKeyframesData**)nxCore::mem_alloc(nmot * sizeof(sxKeyframesData*), XD_FOURCC('M', 'K', 'f', 'r'));
	if (!mppKfrs) return;
	mppRigLinks = (sxKeyframesData::RigLink**)nxCore::mem_alloc(nmot * sizeof(sxKeyframesData::RigLink*), XD_FOURCC('M', 'R', 'i', 'g'));
	if (!mppRigLinks) return;
	int upperBodyRootIdx = rig.find_node("j_Spine");
	int lowerBodyRootIdx = rig.find_node("j_Pelvis");
	int armRootIdL = rig.find_node("j_Shoulder_L");
	int armRootIdR = rig.find_node("j_Shoulder_R");
	int slerpNodes[] = {
		upperBodyRootIdx,
		lowerBodyRootIdx,
		armRootIdL,
		armRootIdR
	};
	for (int i = 0; i < nmot; ++i) {
		const char* pKfrFileName = mpCat->get_file_name(i);
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/%s", pBasePath, pKfrFileName);
		pData = nxData::load(path);
		if (pData) {
			mppKfrs[i] = pData->as<sxKeyframesData>();
			if (mppKfrs[i]) {
				if (dbgInfo) {
					nxCore::dbg_msg("[%d]: %s, #frames=%d\n", i, mpCat->get_name(i), mppKfrs[i]->get_frame_count());
				}
				mppRigLinks[i] = mppKfrs[i]->make_rig_link(rig);
				if (mppRigLinks[i]) {
					for (int j = 0; j < XD_ARY_LEN(slerpNodes); ++j) {
						if (rig.ck_node_idx(slerpNodes[j])) {
							int linkIdx = mppRigLinks[i]->get_rig_map()[slerpNodes[j]];
							if (linkIdx >= 0) {
								mppRigLinks[i]->mNodes[linkIdx].mUseSlerp = true;
							}
						}
					}
				}
			}
		}
	}
}

void MOTION_LIB::reset() {
	if (!mpCat) return;
	int nmot = get_motions_num();
	for (int i = 0; i < nmot; ++i) {
		if (mppKfrs && mppKfrs[i]) {
			nxData::unload(mppKfrs[i]);
		}
		if (mppRigLinks && mppRigLinks[i]) {
			nxCore::mem_free(mppRigLinks[i]);
		}
	}
	if (mppKfrs) {
		nxCore::mem_free(mppKfrs);
		mppKfrs = nullptr;
	}
	if (mppRigLinks) {
		nxCore::mem_free(mppRigLinks);
		mppRigLinks = nullptr;
	}
	nxData::unload(mpCat);
	mpCat = nullptr;
}


void TEXTURE_LIB::init(const char* pBasePath) {
	if (!pBasePath) return;
	char path[XD_MAX_PATH];
	XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/texlib.fcat", pBasePath);
	sxData* pData = nxData::load(path);
	if (!pData) return;
	mpCat = pData->as<sxFileCatalogue>();
	if (!mpCat) return;
	int ntex = mpCat->mFilesNum;
	size_t memsize = ntex * sizeof(GEX_TEX*);
	mppTexs = (GEX_TEX**)nxCore::mem_alloc(memsize, XD_FOURCC('T', 'L', 'i', 'b'));
	if (!mppTexs) return;
	::memset(mppTexs, 0, memsize);
	for (int i = 0; i < ntex; ++i) {
		mppTexs[i] = nullptr;
		const char* pTexFileName = mpCat->get_file_name(i);
		XD_SPRINTF(XD_SPRINTF_BUF(path, sizeof(path)), "%s/%s", pBasePath, pTexFileName);
		pData = nxData::load(path);
		if (pData) {
			sxTextureData* pTex = pData->as<sxTextureData>();
			if (pTex) {
				mppTexs[i] = gexTexCreate(*pTex);
			}
			nxData::unload(pData);
		}
	}
}

void TEXTURE_LIB::reset() {
	if (mpCat && mppTexs) {
		int ntex = mpCat->mFilesNum;
		for (int i = 0; i < ntex; ++i) {
			gexTexDestroy(mppTexs[i]);
		}
	}
	nxCore::mem_free(mppTexs);
	mppTexs = nullptr;
	nxData::unload(mpCat);
	mpCat = nullptr;
}


static cxVec tball_proj(float x, float y, float r) {
	float d = ::hypotf(x, y);
	float t = r / ::sqrtf(2.0f);
	float z;
	if (d < t) {
		z = ::sqrtf(r*r - d*d);
	} else {
		z = (t*t) / d;
	}
	return cxVec(x, y, z);
}

void TRACKBALL::update(float x0, float y0, float x1, float y1) {
	cxVec tp0 = tball_proj(x0, y0, mRadius);
	cxVec tp1 = tball_proj(x1, y1, mRadius);
	cxVec dir = tp0 - tp1;
	cxVec axis = nxVec::cross(tp1, tp0);
	float t = nxCalc::clamp(dir.mag() / (2.0f*mRadius), -1.0f, 1.0f);
	float ang = 2.0f * ::asinf(t);
	mSpin.set_rot(axis, ang);
	mQuat.mul(mSpin);
	mQuat.normalize();
}


SH_COEFS::SH_COEFS() {
	clear();
	calc_diff_wgt(4.0f, 1.0f);
	calc_refl_wgt(8.0f, 0.025f);
}

void SH_COEFS::clear() {
	for (int i = 0; i < NCOEF; ++i) {
		mR[i] = 0.0f;
		mG[i] = 0.0f;
		mB[i] = 0.0f;
	}
	for (int i = 0; i < ORDER; ++i) {
		mWgtDiff[i] = 0.0f;
		mWgtRefl[i] = 0.0f;
	}
}

void SH_COEFS::from_geo(sxGeometryData& geo, float scl) {
	char name[32];
	for (int ch = 0; ch < 3; ++ch) {
		float* pCh = get_channel(ch);
		char chName = "rgb"[ch];
		for (int i = 0; i < NCOEF; ++i) {
			float val = 0.0f;
			XD_SPRINTF(XD_SPRINTF_BUF(name, sizeof(name)), "SHC_%c%d", chName, i);
			int attrIdx = geo.find_glb_attr(name);
			float* pVal = geo.get_attr_data_f(attrIdx, sxGeometryData::eAttrClass::GLOBAL, 0);
			if (pVal) {
				val = *pVal;
			}
			val *= scl;
			pCh[i] = val;
		}
	}
}

void SH_COEFS::scl(const cxColor& clr) {
	for (int i = 0; i < NCOEF; ++i) {
		mR[i] *= clr.r;
		mG[i] *= clr.g;
		mB[i] *= clr.b;
	}
}


float frand01() {
	uxVal32 uf;
	uint32_t r0 = ::rand() & 0xFF;
	uint32_t r1 = ::rand() & 0xFF;
	uint32_t r2 = ::rand() & 0xFF;
	uf.f = 1.0f;
	uf.u |= (r0 | (r1 << 8) | ((r2 & 0x7F) << 16));
	uf.f -= 1.0f;
	return uf.f;
}

float frand11() {
	float r = frand01() - 0.5f;
	return r + r;
}

float cvt_roughness(float rough) {
	float res = rough;
	if (WK.pLib && WK.eidRoughCvt >= 0) {
		sxExprLibData::Entry ent = WK.pLib->get_entry(WK.eidRoughCvt);
		if (ent.is_valid()) {
			const sxCompiledExpression* pExp = ent.get_expr();
			if (pExp && pExp->is_valid()) {
				WK.ctx.mRoughness = rough;
				pExp->exec(WK.ctx);
				res = WK.ctx.mRes;
			}
		}
	}
	return res;
}

cxQuat get_val_grp_quat(sxValuesData::Group& grp) {
	cxVec rot = grp.get_vec("r");
	exRotOrd rord = nxDataUtil::rot_ord_from_str(grp.get_str("rOrd"));
	cxQuat q;
	q.set_rot_degrees(rot, rord);
	return q;
}

cxVec vec_rot_deg(const cxVec& vsrc, float angX, float angY, float angZ, exRotOrd rord) {
	cxQuat q;
	q.set_rot_degrees(cxVec(angX, angY, angZ), rord);
	return q.apply(vsrc);
}

GEX_SPEC_MODE parse_spec_mode(const char* pMode) {
	GEX_SPEC_MODE mode = GEX_SPEC_MODE::PHONG;
	if (nxCore::str_eq(pMode, "phong")) {
		mode = GEX_SPEC_MODE::PHONG;
	} else if (nxCore::str_eq(pMode, "blinn")) {
		mode = GEX_SPEC_MODE::BLINN;
	} else if (nxCore::str_eq(pMode, "ggx")) {
		mode = GEX_SPEC_MODE::GGX;
	}
	return mode;
}

static cxVec fcv_grp_eval(const sxKeyframesData& kfr, const char* pNodeName, float frame, const char** ppChTbl) {
	cxVec val(0.0f);
	if (pNodeName && ppChTbl) {
		for (int i = 0; i < 3; ++i) {
			int idx = kfr.find_fcv_idx(pNodeName, ppChTbl[i]);
			if (idx >= 0) {
				sxKeyframesData::FCurve fcv = kfr.get_fcv(idx);
				if (fcv.is_valid()) {
					val.set_at(i, fcv.eval(frame));
				}
			}
		}
	}
	return val;
}

cxVec eval_pos(const sxKeyframesData& kfr, const char* pNodeName, float frame) {
	static const char* chTbl[] = { "tx", "ty", "tz" };
	return fcv_grp_eval(kfr, pNodeName, frame, chTbl);
}

cxVec eval_rot(const sxKeyframesData& kfr, const char* pNodeName, float frame) {
	static const char* chTbl[] = { "rx", "ry", "rz" };
	return fcv_grp_eval(kfr, pNodeName, frame, chTbl);
}

GEX_LIT* make_lights(const sxValuesData& vals, cxVec* pDominantDir) {
	GEX_LIT* pLit = gexLitCreate();
	cxVec dominantDir = nxVec::get_axis(exAxis::MINUS_Z);
	if (pLit) {
		float maxLum = 0.0f;
		int n = vals.get_grp_num();
		int lidx = 0;
		for (int i = 0; i < n; ++i) {
			sxValuesData::Group grp = vals.get_grp(i);
			if (grp.is_valid()) {
				const char* pTypeName = grp.get_type();
				if (pTypeName == "ambient") {
					cxColor ambClr = grp.get_rgb("light_color", cxColor(0.1f));
					float ambInt = grp.get_float("light_intensity", 1.0f);
					ambClr.scl_rgb(ambInt);
					gexAmbient(pLit, ambClr);
				} else if (nxCore::str_starts_with(pTypeName, "hlight")) {
					if (lidx < D_GEX_MAX_DYN_LIGHTS) {
						cxVec pos = grp.get_vec("t");
						cxColor clr = grp.get_rgb("light_color", cxColor(1.0f));
						float val = grp.get_float("light_intensity", 0.1f);
						clr.scl_rgb(val);
						cxQuat q = get_val_grp_quat(grp);
						cxVec dir = q.apply(nxVec::get_axis(exAxis::MINUS_Z));
						gexLightDir(pLit, lidx, dir);
						gexLightColor(pLit, lidx, clr);
						gexLightMode(pLit, lidx, GEX_LIGHT_MODE::DIST);
						float lum = clr.luminance();
						if (lum > maxLum) {
							dominantDir = dir;
							maxLum = lum;
						}
						++lidx;
					} else {
						nxCore::dbg_msg("Skipping light entry @ %s:%s.", vals.get_name(), grp.get_name());
					}
				}
			}
		}
		gexLitUpdate(pLit);
	}
	if (pDominantDir) {
		*pDominantDir = dominantDir;
	}
	return pLit;
}

GEX_LIT* make_const_lit(const char* pName, float val) {
	GEX_LIT* pLit = gexLitCreate(pName ? pName : "const_lit");
	if (pLit) {
		gexAmbient(pLit, cxColor(val));
		for (int i = 0; i < D_GEX_MAX_DYN_LIGHTS; ++i) {
			gexLightMode(pLit, i, GEX_LIGHT_MODE::NONE);
		}
		gexLitUpdate(pLit);
	}
	return pLit;
}

static GEX_TEX* find_tex(const char* pPath) {
	GEX_TEX* pTex = nullptr;
	if (pPath) {
		size_t len = ::strlen(pPath);
		if (len > 0) {
			int nameOffs = (int)len;
			for (; --nameOffs >= 0;) {
				if (pPath[nameOffs] == '/') break;
			}
			if (nameOffs > 0) {
				pTex = gexTexFind(&pPath[nameOffs + 1]);
			}
		}
	}
	return pTex;
}

void init_materials(GEX_OBJ& obj, const sxValuesData& vals, bool useReflectColor) {
	int n = vals.get_grp_num();
	for (int i = 0; i < n; ++i) {
		sxValuesData::Group grp = vals.get_grp(i);
		if (grp.is_valid()) {
			const char* pMtlName = grp.get_name();
			if (pMtlName) {
				GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
				if (pMtl) {
					float roughness = cvt_roughness(grp.get_float("rough", 0.3f));
					cxColor baseClr = grp.get_rgb("basecolor", cxColor(0.2f));
					GEX_SPEC_MODE specMode = parse_spec_mode(grp.get_str("ogl_spec_model", "phong"));
					float IOR = grp.get_float("ogl_ior_inner", 1.4f);
					gexMtlBaseColor(pMtl, baseClr);
					gexMtlRoughness(pMtl, roughness);
					gexMtlSpecMode(pMtl, specMode);
					gexMtlIOR(pMtl, IOR);
					if (specMode == GEX_SPEC_MODE::GGX) {
						gexMtlSpecFresnelMode(pMtl, 1);
					}
					gexMtlUVMode(pMtl, grp.get_int("ogl_clamping_mode1", 0) ? GEX_UV_MODE::CLAMP : GEX_UV_MODE::WRAP);
					bool baseTexFlg = !!grp.get_int("basecolor_useTexture");
					if (baseTexFlg) {
						const char* pBaseTexName = grp.get_str("basecolor_texture");
						GEX_TEX* pBaseTex = find_tex(pBaseTexName);
						if (pBaseTex) {
							gexMtlBaseTexture(pMtl, pBaseTex);
						}
					}
					bool specTexFlg = !!grp.get_int("reflect_useTexture");
					if (specTexFlg) {
						const char* pSpecTexName = grp.get_str("reflect_texture");
						GEX_TEX* pSpecTex = find_tex(pSpecTexName);
						if (pSpecTex) {
							gexMtlSpecularTexture(pMtl, pSpecTex);
						}
					}
					if (useReflectColor) {
						gexMtlSpecularColor(pMtl, cxColor(grp.get_float("reflect", 0.5f)));
					}
					gexMtlAlpha(pMtl, !!grp.get_int("ogl_cutout"));
					bool bumpTexFlg = !!grp.get_int("enableBumpOrNormalTexture");
					if (bumpTexFlg) {
						gexMtlBumpFactor(pMtl, grp.get_float("normalTexScale", 1.0f));
						gexMtlTangentMode(pMtl, GEX_TANGENT_MODE::AUTO);
						GEX_TEX* pBumpTex = find_tex(grp.get_str("normalTexture", nullptr));
						if (pBumpTex) {
							gexMtlBumpTexture(pMtl, pBumpTex);
							gexMtlBumpMode(pMtl, GEX_BUMP_MODE::NMAP);
						}
					}
					gexMtlUpdate(pMtl);
				}
			}
		}
	}
}

CAM_INFO get_cam_info(const sxValuesData& vals, const char* pCamName) {
	CAM_INFO info;
	info.set_defaults();
	sxValuesData::Group grp = vals.find_grp(pCamName);
	if (grp.is_valid()) {
		info.mPos = grp.get_vec("t", info.mPos);
		info.mQuat = get_val_grp_quat(grp);
		info.mUp = grp.get_vec("up", info.mUp);
		float focal = grp.get_float("focal", 50.0f);
		float aperture = grp.get_float("aperture", 41.4214f);
		info.mFOVY = gexCalcFOVY(focal, aperture);
		info.mNear = grp.get_float("near", info.mNear);
		info.mFar = grp.get_float("far", info.mFar);
	}
	return info;
}

void obj_shadow_mode(const GEX_OBJ& obj, bool castShadows, bool receiveShadows) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlShadowMode(pMtl, castShadows, receiveShadows);
	}
}

void obj_shadow_params(const GEX_OBJ& obj, float density, float selfShadowFactor, bool cullShadows) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlShadowDensity(pMtl, density);
		gexMtlSelfShadowFactor(pMtl, selfShadowFactor);
		gexMtlShadowCulling(pMtl, cullShadows);
		gexMtlUpdate(pMtl);
	}
}

void obj_tesselation(const GEX_OBJ& obj, GEX_TESS_MODE mode, float factor) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlTesselationMode(pMtl, mode);
		gexMtlTesselationFactor(pMtl, factor);
		gexMtlUpdate(pMtl);
	}
}

void obj_diff_mode(const GEX_OBJ& obj, GEX_DIFF_MODE mode) {
	int nmtl = gexObjMtlNum(&obj);
	for (int i = 0; i < nmtl; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlDiffMode(pMtl, mode);
		gexMtlUpdate(pMtl);
	}
}

void obj_sort_mode(const GEX_OBJ& obj, GEX_SORT_MODE mode) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlSortMode(pMtl, mode);
	}
}

void obj_sort_bias(const GEX_OBJ& obj, float absBias, float relBias) {
	int n = gexObjMtlNum(&obj);
	for (int i = 0; i < n; ++i) {
		GEX_MTL* pMtl = gexObjMaterial(&obj, i);
		gexMtlSortBias(pMtl, absBias, relBias);
	}
}

void mtl_sort_mode(const GEX_OBJ& obj, const char* pMtlName, GEX_SORT_MODE mode) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlSortMode(pMtl, mode);
}

void mtl_sort_bias(const GEX_OBJ& obj, const char* pMtlName, float absBias, float relBias) {
	GEX_MTL* pMtl = gexFindMaterial(&obj, pMtlName);
	if (!pMtl) return;
	gexMtlSortBias(pMtl, absBias, relBias);
}
