#include "Scheme.h"

#include <NTL/ZZ.h>
#include <NTL/ZZX.h>
#include <cmath>
#include <vector>

#include "CZZ.h"
#include "NumUtils.h"
#include "Ring2Utils.h"
#include "StringUtils.h"
#include "Params.h"

using namespace std;
using namespace NTL;

//-----------------------------------------

/**
INPUT: level
OUTPUT: integer qi
qi is an integer corresponding to a modulus of ciphertext(c) with c.level = level
*/
ZZ Scheme::getqi(long& level) {
	return params.qi[params.L - level];
}

/**
INPUT: level
OUTPUT: integer Pqi
Pqi is an integer corresponding to a special modulus of ciphertext(c) with c.level = level
This special modulus is used in homomorphic multiplication
*/
ZZ Scheme::getPqi(long& level) {
	return params.Pqi[params.L - level];
}

/**
INPUT: level
OUTPUT: bit size of an integer qi
qi is an integer corresponding to a special modulus of ciphertext(c) with c.level = level
*/
long Scheme::getLogqi(long& level) {
	return params.logq - params.logp * (level-1);
}

//-----------------------------------------

void Scheme::trueValue(CZZ& m, ZZ& qi) {
	while(2 * m.r > qi) m.r -= qi;
	while(2 * m.r < -qi) m.r += qi;

	while(2 * m.i > qi) m.i -= qi;
	while(2 * m.i < -qi) m.i += qi;
}

void Scheme::trueValue(ZZ& m, ZZ& qi) {
	while(2 * m > qi) m -= qi;
	while(2 * m < -qi) m += qi;
}

//-----------------------------------------

/**
INPUT: ZZX bx, ax and ZZ qi
RESULT: Set ax = random in R_qi, bx = ex + ax * sx in R_qi
///> Randomly choose 'ax' in R_qi = Z_qi[X]/(X^N + 1) for N = params.N
*/
void Scheme::rlweInstance(ZZX& ax, ZZX& bx, ZZ& qi) {
	ZZX vx;
	NumUtils::sampleZO(vx, params.N);
	Ring2Utils::mult(ax, vx, publicKey.ax, qi, params.N);
	Ring2Utils::mult(bx, vx, publicKey.bx, qi, params.N);
}

/**
INPUT: ZZX bx, ax
RESULT: Set ax = random in R_qi, bx = ex + ax * sx in R_q for q = params.q
///> Randomly choose 'ax' in R_q = Z_q[X]/(X^N + 1) for N = params.N
*/
void Scheme::rlweInstance(ZZX& ax, ZZX& bx) {
	rlweInstance(ax, bx, params.q);
}

//-----------------------------------------

CZZ* Scheme::groupidx(CZZ*& vals, long slots) {
	CZZ* res = new CZZ[slots * 2];
	long logslots = log2(slots);
	for (long i = 0; i < slots; ++i) {
		res[(params.rotGroup[logslots][i] - 1) / 2] = vals[i];
		res[(params.rotGroupInv[logslots][i] - 1) / 2] = vals[i].conjugate();
	}
	return res;
}

CZZ* Scheme::groupidx(CZZ& val) {
	CZZ* res;
	res = new CZZ[2];
	res[0] = val;
	res[1] = val.conjugate();
	return res;
}

CZZ* Scheme::degroupidx(CZZ*& vals, long slots) {
	long logslots = log2(slots);
	CZZ* res = new CZZ[slots];
	for (long i = 0; i < slots; ++i) {
		res[i] = vals[(params.rotGroup[logslots][i] - 1) / 2];
	}
	return res;
}

//-----------------------------------------

/**
INPUT: array of CZZ values
OUTPUT: encoded Message
*/
Message Scheme::encode(CZZ*& vals, long slots) {
	ZZX mx;
	mx.SetLength(params.N);
	long idx = 0;
	long doubleslots = slots * 2;
	long logDoubleslots = log2(slots) + 1;
	long gap = (params.N >> logDoubleslots);
	CZZ* fftInv = NumUtils::fftSpecialInv(vals, doubleslots, aux.ksiPows, params.logp);
	for (long i = 0; i < doubleslots; ++i) {
		mx.rep[idx] = fftInv[i].r;
		idx += gap;
	}
	return Message(mx, slots);
}

/**
INPUT: msg and level
OUTPUT: ciphertext with modulus qi which is corresponding the input level
///> !!!Before encrypt an array of CZZ values using this function, we have to use encode function.
*/
Cipher Scheme::encryptMsg(Message& msg, long level) {
	ZZX ax, bx;
	ZZ qi = getqi(level);
	rlweInstance(ax, bx, qi);
	Ring2Utils::add(bx, msg.mx, bx, qi, params.N);
	return Cipher(ax, bx, msg.slots, level);
}

Cipher Scheme::encrypt(CZZ*& vals, long slots, long level) {
	CZZ* gvals = groupidx(vals, slots);
	Message msg = encode(gvals, slots);
	return encryptMsg(msg, level);
}

Cipher Scheme::encryptSingle(CZZ& val, long level) {
	CZZ* gvals = groupidx(val);
	Message msg = encode(gvals, 1);
	return encryptMsg(msg, level);
}

//-----------------------------------------

Message Scheme::decryptMsg(SecKey& secretKey, Cipher& cipher) {
	ZZ qi = getqi(cipher.level);
	ZZX mx;
	mx.SetLength(params.N);
	Ring2Utils::mult(mx, cipher.ax, secretKey.sx, qi, params.N);
	Ring2Utils::addAndEqual(mx, cipher.bx, qi, params.N);
	return Message(mx, cipher.slots, cipher.level);
}

CZZ* Scheme::decode(Message& msg) {
	long doubleslots = msg.slots * 2;
	CZZ* fftinv = new CZZ[doubleslots];
	ZZ qi = getqi(msg.level);

	long idx = 0;
	long gap = params.N / doubleslots;
	for (long i = 0; i < doubleslots; ++i) {
		CZZ c(msg.mx.rep[idx], ZZ(0));
		trueValue(c, qi);
		fftinv[i] = c;
		idx += gap;
	}
	return NumUtils::fftSpecial(fftinv, doubleslots, aux.ksiPows, params.logp);
}

CZZ* Scheme::decrypt(SecKey& secretKey, Cipher& cipher) {
	Message msg = decryptMsg(secretKey, cipher);
	CZZ* gvals = decode(msg);
	return degroupidx(gvals, msg.slots);
}

CZZ Scheme::decryptSingle(SecKey& secretKey, Cipher& cipher) {
	Message msg = decryptMsg(secretKey, cipher);
	CZZ* gvals = decode(msg);
	return gvals[0];
}

//-----------------------------------------

Cipher Scheme::add(Cipher& cipher1, Cipher& cipher2) {
	ZZ qi = getqi(cipher1.level);
	ZZX ax, bx;

	Ring2Utils::add(ax, cipher1.ax, cipher2.ax, qi, params.N);
	Ring2Utils::add(bx, cipher1.bx, cipher2.bx, qi, params.N);

	return Cipher(ax, bx, cipher1.slots, cipher1.level);
}

void Scheme::addAndEqual(Cipher& cipher1, Cipher& cipher2) {
	ZZ qi = getqi(cipher1.level);
	Ring2Utils::addAndEqual(cipher1.ax, cipher2.ax, qi, params.N);
	Ring2Utils::addAndEqual(cipher1.bx, cipher2.bx, qi, params.N);
}

//-----------------------------------------

Cipher Scheme::addConst(Cipher& cipher, ZZ& cnst) {
	ZZ qi = getqi(cipher.level);
	ZZX ax = cipher.ax;
	ZZX bx = cipher.bx;

	AddMod(bx.rep[0], cipher.bx.rep[0], cnst, qi);
	bx.normalize();
	return Cipher(ax, bx, cipher.slots, cipher.level);
}

void Scheme::addConstAndEqual(Cipher& cipher, ZZ& cnst) {
	ZZ qi = getqi(cipher.level);
	AddMod(cipher.bx.rep[0], cipher.bx.rep[0], cnst, qi);
	cipher.bx.normalize();
}

//-----------------------------------------

Cipher Scheme::sub(Cipher& cipher1, Cipher& cipher2) {
	ZZ qi = getqi(cipher1.level);
	ZZX ax, bx;

	Ring2Utils::sub(ax, cipher1.ax, cipher2.ax, qi, params.N);
	Ring2Utils::sub(bx, cipher1.bx, cipher2.bx, qi, params.N);

	return Cipher(ax, bx, cipher1.slots, cipher1.level);
}

void Scheme::subAndEqual(Cipher& cipher1, Cipher& cipher2) {
	ZZ qi = getqi(cipher1.level);
	Ring2Utils::subAndEqual(cipher1.ax, cipher2.ax, qi, params.N);
	Ring2Utils::subAndEqual(cipher1.bx, cipher2.bx, qi, params.N);
}

Cipher Scheme::conjugate(Cipher& cipher) {
	ZZ qi = getqi(cipher.level);
	ZZ Pqi = getPqi(cipher.level);

	ZZX bxconj, bxres, axres;

	Ring2Utils::conjugate(bxconj, cipher.bx, params.N);
	Ring2Utils::conjugate(bxres, cipher.ax, params.N);

	Ring2Utils::mult(axres, bxres, publicKey.axConj, Pqi, params.N);
	Ring2Utils::multAndEqual(bxres, publicKey.bxConj, Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axres, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxres, params.logP, params.N);

	Ring2Utils::addAndEqual(bxres, bxconj, qi, params.N);
	return Cipher(axres, bxres, cipher.slots, cipher.level);
}

void Scheme::conjugateAndEqual(Cipher& cipher) {
	ZZ qi = getqi(cipher.level);
	ZZ Pqi = getPqi(cipher.level);

	ZZX bxconj, bxres, axres;

	Ring2Utils::conjugate(bxconj, cipher.bx, params.N);
	Ring2Utils::conjugate(bxres, cipher.ax, params.N);

	Ring2Utils::mult(axres, bxres, publicKey.axConj, Pqi, params.N);
	Ring2Utils::multAndEqual(bxres, publicKey.bxConj, Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axres, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxres, params.logP, params.N);

	Ring2Utils::addAndEqual(bxres, bxconj, qi, params.N);

	cipher.ax = axres;
	cipher.bx = bxres;
}
//-----------------------------------------

/**
Return = cipher1 * cipher2
///> This alogirhtm contain relinearization
///> To controll message size, we need to do Scheme::modSwitch or Scheme::modSwitchOne after this algorithm
*/
Cipher Scheme::mult(Cipher& cipher1, Cipher& cipher2) {
	ZZ qi = getqi(cipher1.level);
	ZZ Pqi = getPqi(cipher1.level);

	ZZX axbx1 = Ring2Utils::add(cipher1.ax, cipher1.bx, qi, params.N);
	ZZX axbx2 = Ring2Utils::add(cipher2.ax, cipher2.bx, qi, params.N);
	Ring2Utils::multAndEqual(axbx1, axbx2, qi, params.N);

	ZZX bxbx = Ring2Utils::mult(cipher1.bx, cipher2.bx, qi, params.N);
	ZZX axax = Ring2Utils::mult(cipher1.ax, cipher2.ax, qi, params.N);

	ZZX axmult = Ring2Utils::mult(axax, publicKey.axStar, Pqi, params.N);
	ZZX bxmult = Ring2Utils::mult(axax, publicKey.bxStar, Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axmult, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxmult, params.logP, params.N);

	Ring2Utils::addAndEqual(axmult, axbx1, qi, params.N);
	Ring2Utils::subAndEqual(axmult, bxbx, qi, params.N);
	Ring2Utils::subAndEqual(axmult, axax, qi, params.N);
	Ring2Utils::addAndEqual(bxmult, bxbx, qi, params.N);

	return Cipher(axmult, bxmult, cipher1.slots, cipher1.level);
}

/**
cipher1 *= cipher2
///> This alogirhtm contain relinearization
///> To controll message size, we need to do Scheme::modSwitch or Scheme::modSwitchOne after this algorithm
*/
void Scheme::multAndEqual(Cipher& cipher1, Cipher& cipher2) {
	ZZ qi = getqi(cipher1.level);
	ZZ Pqi = getPqi(cipher1.level);

	ZZX axbx1 = Ring2Utils::add(cipher1.ax, cipher1.bx, qi, params.N);
	ZZX axbx2 = Ring2Utils::add(cipher2.ax, cipher2.bx, qi, params.N);
	Ring2Utils::multAndEqual(axbx1, axbx2, qi, params.N);

	ZZX bxbx = Ring2Utils::mult(cipher1.bx, cipher2.bx, qi, params.N);
	ZZX axax = Ring2Utils::mult(cipher1.ax, cipher2.ax, qi, params.N);

	cipher1.ax = Ring2Utils::mult(axax, publicKey.axStar, Pqi, params.N);
	cipher1.bx = Ring2Utils::mult(axax, publicKey.bxStar, Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(cipher1.ax, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(cipher1.bx, params.logP, params.N);

	Ring2Utils::addAndEqual(cipher1.ax, axbx1, qi, params.N);
	Ring2Utils::subAndEqual(cipher1.ax, bxbx, qi, params.N);
	Ring2Utils::subAndEqual(cipher1.ax, axax, qi, params.N);
	Ring2Utils::addAndEqual(cipher1.bx, bxbx, qi, params.N);
}

//-----------------------------------------

/**
Return = cipher * cipher
///> This alogirhtm contain relinearization
///> To controll message size, we need to do Scheme::modSwitch or Scheme::modSwitchOne after this algorithm
*/
Cipher Scheme::square(Cipher& cipher) {
	ZZ qi = getqi(cipher.level);
	ZZ Pqi = getPqi(cipher.level);
	ZZX axax, axbx, bxbx, bxmult, axmult;

	Ring2Utils::square(bxbx, cipher.bx, qi, params.N);
	Ring2Utils::mult(axbx, cipher.ax, cipher.bx, qi, params.N);
	Ring2Utils::addAndEqual(axbx, axbx, qi, params.N);
	Ring2Utils::square(axax, cipher.ax, qi, params.N);

	Ring2Utils::mult(axmult, axax, publicKey.axStar, Pqi, params.N);
	Ring2Utils::mult(bxmult, axax, publicKey.bxStar, Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axmult, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxmult, params.logP, params.N);

	Ring2Utils::addAndEqual(axmult, axbx, qi, params.N);
	Ring2Utils::addAndEqual(bxmult, bxbx, qi, params.N);

	return Cipher(axmult, bxmult, cipher.slots, cipher.level);
}

/**
cipher = cipher * cipher
///> This alogirhtm contain relinearization
///> To controll message size, we need to do Scheme::modSwitch or Scheme::modSwitchOne after this algorithm
*/
void Scheme::squareAndEqual(Cipher& cipher) {
	ZZ qi = getqi(cipher.level);
	ZZ Pqi = getPqi(cipher.level);
	ZZX bxbx, axbx, axax, bxmult, axmult;

	Ring2Utils::square(bxbx, cipher.bx, qi, params.N);
	Ring2Utils::mult(axbx, cipher.bx, cipher.ax, qi, params.N);
	Ring2Utils::addAndEqual(axbx, axbx, qi, params.N);
	Ring2Utils::square(axax, cipher.ax, qi, params.N);

	Ring2Utils::mult(axmult, axax, publicKey.axStar, Pqi, params.N);
	Ring2Utils::mult(bxmult, axax, publicKey.bxStar, Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axmult, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxmult, params.logP, params.N);

	Ring2Utils::addAndEqual(axmult, axbx, qi, params.N);
	Ring2Utils::addAndEqual(bxmult, bxbx, qi, params.N);

	cipher.bx = bxmult;
	cipher.ax = axmult;
}

//-----------------------------------------

Cipher Scheme::multByConst(Cipher& cipher, ZZ& cnst) {
	ZZ qi = getqi(cipher.level);
	ZZX ax, bx;
	Ring2Utils::multByConst(ax, cipher.ax, cnst, qi, params.N);
	Ring2Utils::multByConst(bx, cipher.bx, cnst, qi, params.N);

	return Cipher(ax, bx, cipher.slots, cipher.level);
}

void Scheme::multByConstAndEqual(Cipher& cipher, ZZ& cnst) {
	ZZ qi = getqi(cipher.level);
	Ring2Utils::multByConstAndEqual(cipher.ax, cnst, qi, params.N);
	Ring2Utils::multByConstAndEqual(cipher.bx, cnst, qi, params.N);
}

//-----------------------------------------

Cipher Scheme::multByMonomial(Cipher& cipher, const long& degree) {
	ZZX ax, bx;

	Ring2Utils::multByMonomial(ax, cipher.ax, degree, params.N);
	Ring2Utils::multByMonomial(bx, cipher.bx, degree, params.N);

	return Cipher(ax, bx, cipher.slots, cipher.level);
}

void Scheme::multByMonomialAndEqual(Cipher& cipher, const long& degree) {
	Ring2Utils::multByMonomialAndEqual(cipher.ax, degree, params.N);
	Ring2Utils::multByMonomialAndEqual(cipher.bx, degree, params.N);
}

//-----------------------------------------

Cipher Scheme::leftShift(Cipher& cipher, long& bits) {
	long logqi = getLogqi(cipher.level);
	ZZX ax, bx;

	Ring2Utils::leftShift(ax, cipher.ax, bits, logqi, params.N);
	Ring2Utils::leftShift(bx, cipher.bx, bits, logqi, params.N);

	return Cipher(ax, bx, cipher.slots, cipher.level);
}

void Scheme::leftShiftAndEqual(Cipher& cipher, long& bits) {
	long logqi = getLogqi(cipher.level);
	Ring2Utils::leftShiftAndEqual(cipher.ax, bits, logqi, params.N);
	Ring2Utils::leftShiftAndEqual(cipher.bx, bits, logqi, params.N);
}

void Scheme::doubleAndEqual(Cipher& cipher) {
	long logqi = getLogqi(cipher.level);
	Ring2Utils::doubleAndEqual(cipher.ax, logqi, params.N);
	Ring2Utils::doubleAndEqual(cipher.bx, logqi, params.N);
}

//-----------------------------------------

Cipher Scheme::modSwitch(Cipher& cipher, long newLevel) {
	long logdf = params.logp * (newLevel-cipher.level);
	ZZX ax, bx;

	Ring2Utils::rightShift(ax, cipher.ax, logdf, params.N);
	Ring2Utils::rightShift(bx, cipher.bx, logdf, params.N);

	return Cipher(ax, bx, cipher.slots, newLevel);
}

Cipher Scheme::modSwitchOne(Cipher& cipher) {
	long newLevel = cipher.level + 1;
	return modSwitch(cipher, newLevel);
}

void Scheme::modSwitchAndEqual(Cipher& cipher, long newLevel) {
	long logdf = params.logp * (newLevel-cipher.level);
	Ring2Utils::rightShiftAndEqual(cipher.ax, logdf, params.N);
	Ring2Utils::rightShiftAndEqual(cipher.bx, logdf, params.N);
	cipher.level = newLevel;
}

void Scheme::modSwitchOneAndEqual(Cipher& cipher) {
	long newLevel = cipher.level + 1;
	modSwitchAndEqual(cipher, newLevel);
}

//-----------------------------------------

Cipher Scheme::modEmbed(Cipher& cipher, long newLevel) {
	long newLogqi = getLogqi(newLevel);
	ZZX bx, ax;
	Ring2Utils::truncate(ax, cipher.ax, newLogqi, params.N);
	Ring2Utils::truncate(bx, cipher.bx, newLogqi, params.N);

	return Cipher(ax, bx, cipher.slots, newLevel);
}

Cipher Scheme::modEmbed(Cipher& cipher) {
	long newLevel = cipher.level + 1;
	return modEmbed(cipher, newLevel);
}

void Scheme::modEmbedAndEqual(Cipher& cipher, long newLevel) {
	long newLogqi = getLogqi(newLevel);
	Ring2Utils::truncateAndEqual(cipher.ax, newLogqi, params.N);
	Ring2Utils::truncateAndEqual(cipher.bx, newLogqi, params.N);
	cipher.level = newLevel;
}

void Scheme::modEmbedOneAndEqual(Cipher& cipher) {
	long newLevel = cipher.level + 1;
	modEmbedAndEqual(cipher, newLevel);
}

//-----------------------------------------

Cipher Scheme::multAndModSwitchOne(Cipher& cipher1, Cipher& cipher2) {
	Cipher cmult = mult(cipher1, cipher2);
	modSwitchOneAndEqual(cmult);
	return cmult;
}

void Scheme::multModSwitchOneAndEqual(Cipher& cipher1, Cipher& cipher2) {
	multAndEqual(cipher1, cipher2);
	modSwitchOneAndEqual(cipher1);
}

//-----------------------------------------

Cipher Scheme::leftRotateByPo2(Cipher& cipher, long& logPow) {
	ZZ qi = getqi(cipher.level);
	ZZ Pqi = getPqi(cipher.level);

	ZZX bxrot, bxres, axres;

	long pow = (1 << logPow);

	Ring2Utils::inpower(bxrot, cipher.bx, params.rotGroup[params.logNh][pow], params.q, params.N);
	Ring2Utils::inpower(bxres, cipher.ax, params.rotGroup[params.logNh][pow], params.q, params.N);

	Ring2Utils::mult(axres, bxres, publicKey.axKeySwitch[logPow], Pqi, params.N);
	Ring2Utils::multAndEqual(bxres, publicKey.bxKeySwitch[logPow], Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axres, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxres, params.logP, params.N);

	Ring2Utils::addAndEqual(bxres, bxrot, qi, params.N);
	return Cipher(axres, bxres, cipher.slots, cipher.level);
}

void Scheme::leftRotateByPo2AndEqual(Cipher& cipher, long& logPow) {
	ZZ qi = getqi(cipher.level);
	ZZ Pqi = getPqi(cipher.level);

	ZZX bxrot, bxres, axres;

	long pow = (1 << logPow);

	Ring2Utils::inpower(bxrot, cipher.bx, params.rotGroup[params.logNh][pow], params.q, params.N);
	Ring2Utils::inpower(bxres, cipher.ax, params.rotGroup[params.logNh][pow], params.q, params.N);

	Ring2Utils::mult(axres, bxres, publicKey.axKeySwitch[logPow], Pqi, params.N);
	Ring2Utils::multAndEqual(bxres, publicKey.bxKeySwitch[logPow], Pqi, params.N);

	Ring2Utils::rightShiftAndEqual(axres, params.logP, params.N);
	Ring2Utils::rightShiftAndEqual(bxres, params.logP, params.N);

	Ring2Utils::addAndEqual(bxres, bxrot, qi, params.N);

	cipher.ax = axres;
	cipher.bx = bxres;
}

Cipher Scheme::leftRotate(Cipher& cipher, long& steps) {
	Cipher res = cipher;
	leftRotateAndEqual(res, steps);
	return res;
}

void Scheme::leftRotateAndEqual(Cipher& cipher, long& steps) {
	steps %= params.Nh;
	long logsteps = log2(steps);
	for (long i = 0; i < logsteps; ++i) {
		if(bit(steps, i)) {
			leftRotateByPo2AndEqual(cipher, i);
		}
	}
}