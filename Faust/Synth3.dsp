// SPACE TO BE RESONATOR INSTRUMENT v3.0
import("stdfaust.lib");

//-------------------------------------------CLOCKS
bpm=hslider("bpm",81,1,400,1);
barHz=bpm/(60*6);
step=os.phasor(48,barHz):int+1;

//-------------------------------------------UTILS
//___________________SELF BLOCKING ENV
envAR(att,rel,gate) = (_:1,_>0:- <: en.ar((_*gate,att:ba.sAndH),(_*gate,rel:ba.sAndH),(_*gate))) ~ _;
osc(freq) = rdtable(tablesize, os.sinwaveform(tablesize), int(os.phasor(tablesize,freq)))
with{
    tablesize = 1 << 15; // instead of 1 << 16
};

//-------------------------------------------FM-2OPs
//___________________VARIABLES
fmVol = hslider("fmVol",0.5,0,1,0.001):si.smoo;
fmVerb = hslider("fmVerb", 0.14,0,1,0.001):si.smoo;
//fm synth 1
fm1vol=hslider("fm1vol",1,0,1,0.001);
fmNxtFreq1 = hslider("fmFreq1",92,0,127,0.01):ba.midikey2hz;
fmNxtFreq2 = hslider("fmFreq2",92,0,127,0.01):ba.midikey2hz;
fmMm1 = hslider("fmMm1",0.666,0.0,100,0.0001):si.smoo;
fmDp1 = hslider("fmDp1",0.3,0.00,10,0.0001):si.smoo;
fmAM1 = hslider("fmAM1",0.02,0.001,10,0.01);
fmRM1 = hslider("fmRM1",0.2,0.001,10,0.01);
fmAC1 = hslider("fmAC1",0.03,0.001,10,0.01);
fmRC1 = hslider("fmRC1",0.1,0.001,10,0.01);
fmTrigD1 = button("fmTrigD1");
fmGateT1 = checkbox("fmGateT1");
fmEucNo1 = hslider("fmEucNo1", 19, 0, 24, 1);
fmEucNo2 = hslider("fmEucNo2", 19, 0, 24, 1);
fmModWheel1 = hslider("fmModWheel1", 1, 0.1, 100, 0.001):si.smoo;

//fm synth 1 
fmEuc1 = ((step*fmEucNo1) % 48) < fmEucNo1 : en.ar(0,0.001);
fmRawTrig1 = fmTrigD1+(fmEuc1*fmGateT1);
fmTrig1 = envAR(0, fmAC1+fmRC1, fmRawTrig1):en.ar(0,0.001);
fmFreq1 = fmNxtFreq1:ba.sAndH(fmTrig1);
fmac1 = fmAC1:ba.sAndH(fmTrig1);
fmrc1 = fmRC1:ba.sAndH(fmTrig1);
fmam1 = fmAM1:ba.sAndH(fmTrig1);
fmrm1 = fmRM1:ba.sAndH(fmTrig1);
fm2op1(freq,mMul,dMul,trig,am,rm,ac,rc) =  
    osc(
        freq+(  osc(freq*mMul)*freq*dMul*en.ar(am,rm,trig)  )
    )*envAR(ac,rc, trig)*fm1vol;
fmDpm1 = fmDp1*fmModWheel1;
//fm synth 1 
fmEuc2 = ((step*fmEucNo2) % 48) < fmEucNo2 : en.ar(0,0.001);
fmRawTrig2 = fmTrigD1+(fmEuc2*fmGateT1);
fmTrig2 = envAR(0, fmAC1+fmRC1, fmRawTrig2):en.ar(0,0.001);
fmFreq2 = fmNxtFreq2:ba.sAndH(fmTrig2);


//fm group
dry = 1-(fmVerb);
fmSynths = fmVol*(
    fm2op1(fmFreq1,fmMm1,fmDpm1,fmTrig1, fmam1,fmrm1,fmac1,fmrc1)+
    fm2op1(fmFreq2,fmMm1,fmDpm1,fmTrig2, fmam1,fmrm1,fmac1,fmrc1)
    )<: (_*dry),(_*fmVerb) : _,(_ : re.mono_freeverb(0.88,0.88,0.8,0)) :+;
// re.mono_freeverb(0.88,0.2,0.8,0)
//-------------------------------------------KICK/DRONEs
//___________________VARIABLES
kroneVol = hslider("kroneVol",0.5,0,1,0.001);
//krone1
kd1vol=hslider("kd1vol",1,0,1,0.001);
kdTrig1 = button("kdTrig1");
kdNxtFreq1 = hslider("kdNxtFreq1", 40, 0, 127, 1):ba.midikey2hz;
kdDelta1 = hslider("kdDelta1", -.79,-0.99,5,0.001);
kdA1 = hslider("kdA1", 0.02, 0, 15, 0.01);
kdR1 = hslider("kdR1", 0.15, 0, 15, 0.01);
kdTrigD1 = button("kdTrigD1");
kdGateT1 =  checkbox("kdGateT1");
kdEucNo1 = hslider("kdEucNo1", 6, 0, 24, 1);

//krone2
kd2vol=hslider("kd2vol",1,0,1,0.001);
kdTrig2 = button("kdTrig2");
kdNxtFreq2 = hslider("kdNxtFreq2", 38, 0, 127, 1):ba.midikey2hz;
kdDelta2 = hslider("kdDelta2", 0,-0.99,5,0.001);
kdA2 = hslider("kdA2", 0.02, 0, 15, 0.01);
kdR2 = hslider("kdR2", 3, 0, 15, 0.01);
kdTrigD2 = button("kdTrigD2");
kdGateT2 = checkbox("kdGateT2");
kdEucNo2 = hslider("kdEucNo2", 1, 0, 24, 1);

//___________________DSP
//krone1
kdEuc1 = ((step*kdEucNo1) % 48) < kdEucNo1 : en.ar(0,0.001);
kdRawTrig1 = kdTrig1+(kdEuc1*kdGateT1); 
kdEnvT1 = envAR(kdA1+kdR1, 0, kdRawTrig1);
kdGate1 = 1-en.ar(0,0.001,kdEnvT1);
kdFreq1 = kdNxtFreq1:ba.sAndH(kdGate1);
kddelta1 = kdDelta1:ba.sAndH(kdGate1);
kdRamp1 = kdFreq1+(kdFreq1*kddelta1*kdEnvT1);
kdEnv1 = envAR(kdA1, kdR1, kdRawTrig1);
krone1 = os.triangle(kdRamp1)*kdEnv1*kd1vol;

//krone2
kdEuc2 = ((step*kdEucNo2) % 48) < kdEucNo2 : en.ar(0,0.001);
kdRawTrig2 = kdTrig2+(kdEuc2*kdGateT2); 
kdEnvT2 = envAR(kdA2+kdR2, 0, kdRawTrig2);
kdGate2 = 1-en.ar(0,0.001,kdEnvT2);
kdFreq2 = kdNxtFreq2:ba.sAndH(kdGate2);
kddelta2 = kdDelta2:ba.sAndH(kdGate2);
kdRamp2 = kdFreq2+(kdFreq2*kddelta2*kdEnvT2);
kdEnv2 = envAR(kdA2, kdR2, kdRawTrig2);
krone2 = (os.triangle(kdRamp2*2)+os.triangle(kdRamp2))*kdEnv2*kd2vol:fi.lowpass(1, 600);

//-------------------------------------------ADRIFT
adriftVol = hslider("adriftVol", 0, 0, 1, 0.001)^2:si.smooth(ba.tau2pole(1));
adriftFreq = hslider("adriftFreq", 0, 0, 1, 0.001)*1000+120:si.smoo;
noise = no.noise*0.1;
filtNoise1 = noise:fi.lowpass6e(adriftFreq):fi.highpass6e(100);
adrift = filtNoise1*adriftVol;

//-------------------------------------------OUTPUT
masterVol = hslider("masterVol", 0.5, 0, 1, 0.001):si.smoo;
high = fmSynths+adrift:co.compressor_mono(8,-8,0.02,0.02);
low = krone1+(krone1+krone2+(high:fi.lowpass6e(600)*0.7):co.compressor_mono(8,-15,0.02,0.02)):co.compressor_mono(8,-8,0.02,0.02);
process = high*masterVol, low*masterVol;
