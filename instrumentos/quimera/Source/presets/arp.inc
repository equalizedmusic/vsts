// ARP — 40 presets com o arpejador ligado (gerado por tools/gerar-arp.py; mexa lá e gere de novo)
    p.push_back (mk (cArp, u8 ("Escada de Neon"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 5 }, { O (0, "det"), 22 }, { O (0, "wid"), 85 },
          { F (0, "cut"), 700 }, { F (0, "res"), 25 }, { F (0, "env"), 55 }, { E (1, "d"), 220 }, { E (1, "s"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 380 }, { E (0, "s"), 0 }, { E (0, "r"), 250 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d316 }, { id::dlyFb, 40 }, { id::dlyPing, 1 }, { id::dlyMix, 22 }, { id::revOn, 1 },
          { id::revType, rv::placa }, { id::revMix, 15 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpUp }, { id::arpRate, dv::d16 }, { id::arpOct, 2 }, { id::arpGate, 45 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 }, { id::arpScale, 2 }, { id::arpRoot, 9 },
          { arpAccId (0), 1 }, { arpAccId (4), 1 }, { arpAccId (8), 1 }, { arpAccId (12), 1 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstFACut, 30 }, { srcMacro2, dstFARes, 25 }, { srcWheel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Trem das Seis"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 5 }, { O (0, "det"), 22 }, { O (0, "wid"), 85 },
          { F (0, "cut"), 900 }, { F (0, "res"), 25 }, { F (0, "env"), 55 }, { E (1, "d"), 220 }, { E (1, "s"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 380 }, { E (0, "s"), 0 }, { E (0, "r"), 250 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d316 }, { id::dlyFb, 40 }, { id::dlyPing, 1 }, { id::dlyMix, 22 }, { id::revOn, 1 },
          { id::revType, rv::placa }, { id::revMix, 15 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpUpDown }, { id::arpRate, dv::d16 }, { id::arpOct, 3 }, { id::arpGate, 55 },
          { id::arpSwing, 20 }, { id::arpLen, 16 }, { id::arpAcc, 50 }, { arpStepId (1), 0 }, { arpStepId (5), 0 },
          { arpStepId (9), 0 }, { arpStepId (13), 0 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstFACut, 30 }, { srcMacro2, dstFARes, 25 }, { srcWheel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Varal de Luz"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 7 }, { O (0, "det"), 22 }, { O (0, "wid"), 85 },
          { F (0, "cut"), 700 }, { F (0, "res"), 25 }, { F (0, "env"), 55 }, { E (1, "d"), 220 }, { E (1, "s"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 380 }, { E (0, "s"), 0 }, { E (0, "r"), 250 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d8t }, { id::dlyFb, 40 }, { id::dlyPing, 1 }, { id::dlyMix, 22 }, { id::revOn, 1 },
          { id::revType, rv::placa }, { id::revMix, 15 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpAsPlayed }, { id::arpRate, dv::d8t }, { id::arpOct, 1 }, { id::arpGate, 60 },
          { id::arpSwing, 0 }, { id::arpLen, 12 }, { id::arpAcc, 50 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstFACut, 30 }, { srcMacro2, dstFARes, 25 }, { srcWheel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Catraca Trance"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 5 }, { O (0, "det"), 22 }, { O (0, "wid"), 85 },
          { F (0, "cut"), 700 }, { F (0, "res"), 45 }, { F (0, "env"), 55 }, { E (1, "d"), 220 }, { E (1, "s"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 380 }, { E (0, "s"), 0 }, { E (0, "r"), 250 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d316 }, { id::dlyFb, 40 }, { id::dlyPing, 1 }, { id::dlyMix, 22 }, { id::revOn, 1 },
          { id::revType, rv::placa }, { id::revMix, 15 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpConverge }, { id::arpRate, dv::d16 }, { id::arpOct, 2 }, { id::arpGate, 40 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 60 }, { arpAccId (2), 1 }, { arpAccId (6), 1 },
          { arpAccId (10), 1 }, { arpAccId (14), 1 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstFACut, 30 }, { srcMacro2, dstFARes, 25 }, { srcWheel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Ponte Aérea"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 5 }, { O (0, "det"), 22 }, { O (0, "wid"), 85 },
          { F (0, "cut"), 700 }, { F (0, "res"), 25 }, { F (0, "env"), 55 }, { E (1, "d"), 220 }, { E (1, "s"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 380 }, { E (0, "s"), 0 }, { E (0, "r"), 250 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d316 }, { id::dlyFb, 40 }, { id::dlyPing, 1 }, { id::dlyMix, 22 }, { id::revOn, 1 },
          { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -11 }, { O (0, "oct"), 1 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpDown }, { id::arpRate, dv::d32 }, { id::arpOct, 2 }, { id::arpGate, 35 },
          { id::arpSwing, 0 }, { id::arpLen, 8 }, { id::arpAcc, 50 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstFACut, 30 }, { srcMacro2, dstFARes, 25 }, { srcWheel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Ácido em Fila"),
        { { O (0, "pos"), cl::serra }, { F (0, "cut"), 260 }, { F (0, "res"), 80 }, { F (0, "drv"), 30 },
          { F (0, "env"), 70 }, { E (1, "d"), 180 }, { E (0, "a"), 1 }, { E (0, "r"), 60 }, { id::polyMode, pm::legato },
          { id::glide, 60 }, { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 25 }, { id::dstMix, 50 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpUp }, { id::arpRate, dv::d16 },
          { id::arpOct, 2 }, { id::arpGate, 70 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 70 },
          { arpAccId (0), 1 }, { arpAccId (3), 1 }, { arpAccId (6), 1 }, { arpAccId (10), 1 }, { arpAccId (13), 1 } },
        { { srcVel, dstFACut, 35 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFARes, 20 }, { srcMacro3, dstFADrive, 40 } }));

    p.push_back (mk (cArp, u8 ("Cobra Coral"),
        { { O (0, "pos"), cl::serra }, { F (0, "cut"), 260 }, { F (0, "res"), 86 }, { F (0, "drv"), 30 },
          { F (0, "env"), 70 }, { E (1, "d"), 180 }, { E (0, "a"), 1 }, { E (0, "r"), 60 }, { id::polyMode, pm::legato },
          { id::glide, 60 }, { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 25 }, { id::dstMix, 50 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpRandom }, { id::arpRate, dv::d16 },
          { id::arpOct, 2 }, { id::arpGate, 100 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 },
          { arpStepId (1), 0 }, { arpStepId (5), 0 }, { arpStepId (9), 0 }, { arpStepId (11), 0 }, { arpStepId (15), 0 },
          { arpAccId (0), 1 }, { arpAccId (4), 1 }, { arpAccId (10), 1 } },
        { { srcVel, dstFACut, 35 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFARes, 20 }, { srcMacro3, dstFADrive, 40 } }));

    p.push_back (mk (cArp, u8 ("Gotejamento Ácido"),
        { { O (0, "pos"), cl::quadrada }, { F (0, "cut"), 260 }, { F (0, "res"), 80 }, { F (0, "drv"), 30 },
          { F (0, "env"), 70 }, { E (1, "d"), 180 }, { E (0, "a"), 1 }, { E (0, "r"), 60 }, { id::polyMode, pm::legato },
          { id::glide, 60 }, { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 25 }, { id::dstMix, 50 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpAsPlayed }, { id::arpRate, dv::d16 },
          { id::arpOct, 1 }, { id::arpGate, 60 }, { id::arpSwing, 25 }, { id::arpLen, 16 }, { id::arpAcc, 65 },
          { arpAccId (1), 1 }, { arpAccId (4), 1 }, { arpAccId (7), 1 }, { arpAccId (9), 1 }, { arpAccId (12), 1 },
          { arpAccId (15), 1 } },
        { { srcVel, dstFACut, 35 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFARes, 20 }, { srcMacro3, dstFADrive, 40 } }));

    p.push_back (mk (cArp, u8 ("Rua Augusta 4h"),
        { { O (0, "pos"), cl::serra }, { F (0, "cut"), 260 }, { F (0, "res"), 80 }, { F (0, "drv"), 30 },
          { F (0, "env"), 70 }, { E (1, "d"), 180 }, { E (0, "a"), 1 }, { E (0, "r"), 60 }, { id::polyMode, pm::legato },
          { id::glide, 60 }, { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 25 }, { id::dstMix, 50 },
          { id::volume, -11 }, { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 30 }, { id::dlyMix, 15 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpDownUp }, { id::arpRate, dv::d16 }, { id::arpOct, 2 },
          { id::arpGate, 80 }, { id::arpSwing, 0 }, { id::arpLen, 7 }, { id::arpAcc, 50 }, { arpAccId (0), 1 } },
        { { srcVel, dstFACut, 35 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFARes, 20 }, { srcMacro3, dstFADrive, 40 } }));

    p.push_back (mk (cArp, u8 ("Limalha"),
        { { O (0, "pos"), cl::serra }, { F (0, "cut"), 260 }, { F (0, "res"), 80 }, { F (0, "drv"), 55 },
          { F (0, "env"), 70 }, { E (1, "d"), 180 }, { E (0, "a"), 1 }, { E (0, "r"), 60 }, { id::polyMode, pm::legato },
          { id::glide, 60 }, { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 40 }, { id::dstMix, 50 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpDiverge }, { id::arpRate, dv::d16t },
          { id::arpOct, 2 }, { id::arpGate, 60 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 60 },
          { arpAccId (0), 1 }, { arpAccId (4), 1 }, { arpAccId (8), 1 }, { arpAccId (12), 1 } },
        { { srcVel, dstFACut, 35 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFARes, 20 }, { srcMacro3, dstFADrive, 40 } }));

    p.push_back (mk (cArp, u8 ("Carrilhão de Bolso"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 40 }, { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal },
          { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 }, { F (0, "cut"), 9000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 700 }, { E (2, "d"), 600 },
          { E (2, "s"), 0 }, { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 3.5f }, { id::revMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 35 }, { id::dlyMix, 18 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpUp }, { id::arpRate, dv::d8 }, { id::arpOct, 2 },
          { id::arpGate, 80 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstO2Level, 50 } }));

    p.push_back (mk (cArp, u8 ("Sino na Escada"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 40 }, { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal },
          { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 50 }, { F (0, "cut"), 9000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 700 }, { E (2, "d"), 600 },
          { E (2, "s"), 0 }, { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 3.5f }, { id::revMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 35 }, { id::dlyMix, 18 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpUpDown }, { id::arpRate, dv::d16 }, { id::arpOct, 3 },
          { id::arpGate, 60 }, { id::arpSwing, 30 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstO2Level, 50 } }));

    p.push_back (mk (cArp, u8 ("Caixinha de Música"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 40 }, { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal },
          { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 }, { F (0, "cut"), 9000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 700 }, { E (2, "d"), 600 },
          { E (2, "s"), 0 }, { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 3.5f }, { id::revMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 35 }, { id::dlyMix, 10 }, { id::volume, -12 },
          { O (0, "oct"), 1 }, { id::arpOn, 1 }, { id::arpMode, (float) arpRandom }, { id::arpRate, dv::d8t },
          { id::arpOct, 2 }, { id::arpGate, 70 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 },
          { id::arpScale, 10 }, { id::arpRoot, 0 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstO2Level, 50 } }));

    p.push_back (mk (cArp, u8 ("Relógio de Torre"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 40 }, { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal },
          { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 }, { F (0, "cut"), 9000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 700 }, { E (2, "d"), 600 },
          { E (2, "s"), 0 }, { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 3.5f }, { id::revMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 35 }, { id::dlyMix, 18 }, { id::volume, -12 },
          { O (0, "oct"), -1 }, { id::arpOn, 1 }, { id::arpMode, (float) arpDiverge }, { id::arpRate, dv::d16 },
          { id::arpOct, 2 }, { id::arpGate, 65 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 },
          { arpStepId (1), 0 }, { arpStepId (2), 0 }, { arpStepId (4), 0 }, { arpStepId (5), 0 }, { arpStepId (7), 0 },
          { arpStepId (9), 0 }, { arpStepId (10), 0 }, { arpStepId (12), 0 }, { arpStepId (13), 0 },
          { arpStepId (15), 0 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstO2Level, 50 } }));

    p.push_back (mk (cArp, u8 ("Orvalho de Metal"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 25 }, { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal },
          { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 }, { F (0, "cut"), 9000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 700 }, { E (2, "d"), 600 },
          { E (2, "s"), 0 }, { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 3.5f }, { id::revMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 35 }, { id::dlyMix, 18 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpDown }, { id::arpRate, dv::d4 }, { id::arpOct, 3 },
          { id::arpGate, 90 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstO2Level, 50 } }));

    p.push_back (mk (cArp, u8 ("Acorde em Rajada"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 7 }, { O (0, "det"), 30 }, { O (0, "wid"), 95 }, { O (1, "on"), 1 },
          { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -1 }, { O (1, "lvl"), 45 },
          { F (0, "cut"), 900 }, { F (0, "res"), 30 }, { F (0, "env"), 50 }, { E (1, "d"), 260 }, { E (0, "a"), 1 },
          { E (0, "d"), 300 }, { E (0, "s"), 30 }, { E (0, "r"), 200 }, { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 20 }, { id::volume, -14 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpChord }, { id::arpRate, dv::d16 }, { id::arpOct, 1 }, { id::arpGate, 30 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 60 }, { arpStepId (1), 0 }, { arpStepId (5), 0 },
          { arpStepId (9), 0 }, { arpStepId (13), 0 }, { arpAccId (0), 1 }, { arpAccId (4), 1 }, { arpAccId (8), 1 },
          { arpAccId (12), 1 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cArp, u8 ("Stab de Galpão"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 7 }, { O (0, "det"), 30 }, { O (0, "wid"), 95 }, { O (1, "on"), 1 },
          { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -1 }, { O (1, "lvl"), 45 },
          { F (0, "cut"), 1300 }, { F (0, "res"), 30 }, { F (0, "env"), 50 }, { E (1, "d"), 260 }, { E (0, "a"), 1 },
          { E (0, "d"), 300 }, { E (0, "s"), 30 }, { E (0, "r"), 200 }, { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 20 }, { id::volume, -14 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d316 }, { id::dlyMix, 20 }, { id::arpOn, 1 }, { id::arpMode, (float) arpChord },
          { id::arpRate, dv::d8 }, { id::arpOct, 2 }, { id::arpGate, 40 }, { id::arpSwing, 0 }, { id::arpLen, 16 },
          { id::arpAcc, 50 }, { arpStepId (0), 0 }, { arpStepId (2), 0 }, { arpStepId (4), 0 }, { arpStepId (6), 0 },
          { arpStepId (8), 0 }, { arpStepId (10), 0 }, { arpStepId (12), 0 }, { arpStepId (14), 0 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cArp, u8 ("Bate-Estaca"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 7 }, { O (0, "det"), 30 }, { O (0, "wid"), 95 }, { O (1, "on"), 1 },
          { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -1 }, { O (1, "lvl"), 45 },
          { F (0, "cut"), 900 }, { F (0, "res"), 30 }, { F (0, "env"), 50 }, { E (1, "d"), 260 }, { E (0, "a"), 1 },
          { E (0, "d"), 300 }, { E (0, "s"), 30 }, { E (0, "r"), 200 }, { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 20 }, { id::volume, -14 }, { id::cmpOn, 1 },
          { id::cmpMode, cp::multibanda }, { id::cmpAmt, 35 }, { id::arpOn, 1 }, { id::arpMode, (float) arpChord },
          { id::arpRate, dv::d16 }, { id::arpOct, 1 }, { id::arpGate, 25 }, { id::arpSwing, 0 }, { id::arpLen, 16 },
          { id::arpAcc, 70 }, { arpStepId (1), 0 }, { arpStepId (2), 0 }, { arpStepId (4), 0 }, { arpStepId (5), 0 },
          { arpStepId (7), 0 }, { arpStepId (9), 0 }, { arpStepId (10), 0 }, { arpStepId (12), 0 }, { arpStepId (13), 0 },
          { arpStepId (15), 0 }, { arpAccId (0), 1 }, { arpAccId (8), 1 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cArp, u8 ("Pulso de Acordes"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 5 }, { O (0, "det"), 30 }, { O (0, "wid"), 95 }, { O (1, "on"), 1 },
          { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -1 }, { O (1, "lvl"), 45 },
          { F (0, "cut"), 900 }, { F (0, "res"), 30 }, { F (0, "env"), 50 }, { E (1, "d"), 260 }, { E (0, "a"), 1 },
          { E (0, "d"), 300 }, { E (0, "s"), 30 }, { E (0, "r"), 200 }, { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 20 }, { id::volume, -14 }, { id::hypOn, 1 },
          { id::hypMix, 35 }, { id::arpOn, 1 }, { id::arpMode, (float) arpChord }, { id::arpRate, dv::d16t },
          { id::arpOct, 1 }, { id::arpGate, 35 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 },
          { arpStepId (1), 0 }, { arpStepId (3), 0 }, { arpStepId (5), 0 }, { arpStepId (7), 0 }, { arpStepId (9), 0 },
          { arpStepId (11), 0 }, { arpStepId (13), 0 }, { arpStepId (15), 0 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cArp, u8 ("Coro de Martelos"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 7 }, { O (0, "det"), 30 }, { O (0, "wid"), 95 }, { O (1, "on"), 0 },
          { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -1 }, { O (1, "lvl"), 45 },
          { F (0, "cut"), 900 }, { F (0, "res"), 30 }, { F (0, "env"), 70 }, { E (1, "d"), 260 }, { E (0, "a"), 1 },
          { E (0, "d"), 300 }, { E (0, "s"), 30 }, { E (0, "r"), 200 }, { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 20 }, { id::volume, -14 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpChord }, { id::arpRate, dv::d8t }, { id::arpOct, 2 }, { id::arpGate, 50 },
          { id::arpSwing, 0 }, { id::arpLen, 6 }, { id::arpAcc, 50 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cArp, u8 ("Cristal Giratório"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 50 }, { O (0, "uni"), 3 }, { O (0, "det"), 15 },
          { F (0, "cut"), 12000 }, { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 10 }, { E (0, "r"), 900 },
          { L (0, "div"), dv::d2 }, { L (0, "mode"), lm::livre }, { id::revOn, 1 }, { id::revType, rv::hall },
          { id::revMix, 35 }, { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyPing, 1 }, { id::dlyFb, 50 },
          { id::dlyMix, 25 }, { id::volume, -12 }, { id::arpOn, 1 }, { id::arpMode, (float) arpUp },
          { id::arpRate, dv::d32 }, { id::arpOct, 3 }, { id::arpGate, 30 }, { id::arpSwing, 0 }, { id::arpLen, 16 },
          { id::arpAcc, 50 }, { id::arpScale, 5 }, { id::arpRoot, 5 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcVel, dstO1Pos, 25 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Gota em Espiral"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 70 }, { O (0, "uni"), 3 }, { O (0, "det"), 15 },
          { F (0, "cut"), 12000 }, { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 10 }, { E (0, "r"), 900 },
          { L (0, "div"), dv::d2 }, { L (0, "mode"), lm::livre }, { id::revOn, 1 }, { id::revType, rv::hall },
          { id::revMix, 35 }, { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyPing, 1 }, { id::dlyFb, 50 },
          { id::dlyMix, 25 }, { id::volume, -12 }, { id::arpOn, 1 }, { id::arpMode, (float) arpUpDown },
          { id::arpRate, dv::d16 }, { id::arpOct, 4 }, { id::arpGate, 50 }, { id::arpSwing, 0 }, { id::arpLen, 16 },
          { id::arpAcc, 50 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcVel, dstO1Pos, 25 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Céu de Lantejoula"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 50 }, { O (0, "uni"), 3 }, { O (0, "det"), 15 },
          { F (0, "cut"), 12000 }, { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 10 }, { E (0, "r"), 900 },
          { L (0, "div"), dv::d2 }, { L (0, "mode"), lm::livre }, { id::revOn, 1 }, { id::revType, rv::hall },
          { id::revMix, 50 }, { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyPing, 1 }, { id::dlyFb, 50 },
          { id::dlyMix, 25 }, { id::volume, -12 }, { id::arpOn, 1 }, { id::arpMode, (float) arpRandom },
          { id::arpRate, dv::d32 }, { id::arpOct, 2 }, { id::arpGate, 40 }, { id::arpSwing, 0 }, { id::arpLen, 16 },
          { id::arpAcc, 50 }, { arpStepId (2), 0 }, { arpStepId (6), 0 }, { arpStepId (9), 0 }, { arpStepId (13), 0 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcVel, dstO1Pos, 25 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Prisma Lento"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 50 }, { O (0, "uni"), 3 }, { O (0, "det"), 15 },
          { F (0, "cut"), 12000 }, { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 10 }, { E (0, "r"), 1500 },
          { L (0, "div"), dv::d2 }, { L (0, "mode"), lm::livre }, { id::revOn, 1 }, { id::revType, rv::hall },
          { id::revMix, 35 }, { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyPing, 1 }, { id::dlyFb, 50 },
          { id::dlyMix, 25 }, { id::volume, -12 }, { id::arpOn, 1 }, { id::arpMode, (float) arpConverge },
          { id::arpRate, dv::d16 }, { id::arpOct, 3 }, { id::arpGate, 55 }, { id::arpSwing, 15 }, { id::arpLen, 16 },
          { id::arpAcc, 50 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcVel, dstO1Pos, 25 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Chuva de Agulhas"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 60 }, { O (0, "uni"), 3 }, { O (0, "det"), 15 },
          { F (0, "cut"), 12000 }, { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 10 }, { E (0, "r"), 900 },
          { L (0, "div"), dv::d2 }, { L (0, "mode"), lm::livre }, { id::revOn, 1 }, { id::revType, rv::hall },
          { id::revMix, 35 }, { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyPing, 1 }, { id::dlyFb, 50 },
          { id::dlyMix, 25 }, { id::volume, -12 }, { id::arpOn, 1 }, { id::arpMode, (float) arpAsPlayed },
          { id::arpRate, dv::d8 }, { id::arpOct, 2 }, { id::arpGate, 70 }, { id::arpSwing, 0 }, { id::arpLen, 16 },
          { id::arpAcc, 50 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcVel, dstO1Pos, 25 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Órgão Saltitante"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 40 }, { F (0, "cut"), 6000 }, { E (0, "a"), 2 },
          { E (0, "d"), 200 }, { E (0, "s"), 70 }, { E (0, "r"), 120 }, { id::choOn, 1 }, { id::choMix, 30 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpUp }, { id::arpRate, dv::d8 },
          { id::arpOct, 1 }, { id::arpGate, 70 }, { id::arpSwing, 35 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcMacro1, dstO1Pos, 50 }, { srcVel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Quermesse"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 65 }, { F (0, "cut"), 6000 }, { E (0, "a"), 2 },
          { E (0, "d"), 200 }, { E (0, "s"), 70 }, { E (0, "r"), 120 }, { id::choOn, 1 }, { id::choMix, 30 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpUpDown }, { id::arpRate, dv::d8t },
          { id::arpOct, 2 }, { id::arpGate, 60 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcMacro1, dstO1Pos, 50 }, { srcVel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Realejo"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 40 }, { F (0, "cut"), 6000 }, { E (0, "a"), 2 },
          { E (0, "d"), 200 }, { E (0, "s"), 70 }, { E (0, "r"), 120 }, { id::choOn, 1 }, { id::choMix, 50 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpAsPlayed }, { id::arpRate, dv::d16 },
          { id::arpOct, 1 }, { id::arpGate, 55 }, { id::arpSwing, 40 }, { id::arpLen, 16 }, { id::arpAcc, 40 },
          { arpAccId (0), 1 }, { arpAccId (4), 1 }, { arpAccId (8), 1 }, { arpAccId (12), 1 } },
        { { srcMacro1, dstO1Pos, 50 }, { srcVel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Igrejinha em Fuga"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 40 }, { F (0, "cut"), 6000 }, { E (0, "a"), 2 },
          { E (0, "d"), 200 }, { E (0, "s"), 70 }, { E (0, "r"), 120 }, { id::choOn, 1 }, { id::choMix, 30 },
          { id::volume, -11 }, { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpDown }, { id::arpRate, dv::d16 }, { id::arpOct, 2 }, { id::arpGate, 65 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcMacro1, dstO1Pos, 50 }, { srcVel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Tecla de Feira"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 20 }, { F (0, "cut"), 6000 }, { E (0, "a"), 2 },
          { E (0, "d"), 200 }, { E (0, "s"), 70 }, { E (0, "r"), 120 }, { id::choOn, 1 }, { id::choMix, 30 },
          { id::volume, -11 }, { id::arpOn, 1 }, { id::arpMode, (float) arpChord }, { id::arpRate, dv::d8 },
          { id::arpOct, 1 }, { id::arpGate, 40 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 },
          { arpStepId (0), 0 }, { arpStepId (2), 0 }, { arpStepId (4), 0 }, { arpStepId (6), 0 }, { arpStepId (8), 0 },
          { arpStepId (10), 0 }, { arpStepId (12), 0 }, { arpStepId (14), 0 } },
        { { srcMacro1, dstO1Pos, 50 }, { srcVel, dstFACut, 15 } }));

    p.push_back (mk (cArp, u8 ("Coral Picado"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 30 }, { O (0, "uni"), 3 }, { O (0, "det"), 18 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { E (0, "a"), 2 }, { E (0, "d"), 350 },
          { E (0, "s"), 20 }, { E (0, "r"), 300 }, { L (1, "div"), dv::c2 }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpUp }, { id::arpRate, dv::d8 }, { id::arpOct, 2 }, { id::arpGate, 90 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcLfo2, dstO1Pos, 35 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Sílabas no Vento"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 55 }, { O (0, "uni"), 3 }, { O (0, "det"), 18 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { E (0, "a"), 2 }, { E (0, "d"), 350 },
          { E (0, "s"), 20 }, { E (0, "r"), 300 }, { L (1, "div"), dv::c2 }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpRandom }, { id::arpRate, dv::d16 }, { id::arpOct, 2 }, { id::arpGate, 70 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 }, { arpStepId (1), 0 }, { arpStepId (4), 0 },
          { arpStepId (7), 0 }, { arpStepId (9), 0 }, { arpStepId (12), 0 }, { arpStepId (15), 0 } },
        { { srcLfo2, dstO1Pos, 35 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Vozes em Roda"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 30 }, { O (0, "uni"), 3 }, { O (0, "det"), 18 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { E (0, "a"), 2 }, { E (0, "d"), 350 },
          { E (0, "s"), 20 }, { E (0, "r"), 300 }, { L (1, "div"), dv::c2 }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -11 }, { id::choOn, 1 },
          { id::choMix, 30 }, { id::arpOn, 1 }, { id::arpMode, (float) arpDownUp }, { id::arpRate, dv::d8t },
          { id::arpOct, 2 }, { id::arpGate, 75 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcLfo2, dstO1Pos, 35 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Mantra em Degraus"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 30 }, { O (0, "uni"), 3 }, { O (0, "det"), 18 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { E (0, "a"), 40 }, { E (0, "d"), 350 },
          { E (0, "s"), 20 }, { E (0, "r"), 300 }, { L (1, "div"), dv::c2 }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 40 }, { id::volume, -11 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpUpDown }, { id::arpRate, dv::d4 }, { id::arpOct, 1 }, { id::arpGate, 100 },
          { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 }, { id::arpScale, 3 }, { id::arpRoot, 2 } },
        { { srcLfo2, dstO1Pos, 35 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Eco de Catedral"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 30 }, { O (0, "uni"), 3 }, { O (0, "det"), 18 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { E (0, "a"), 2 }, { E (0, "d"), 350 },
          { E (0, "s"), 20 }, { E (0, "r"), 300 }, { L (1, "div"), dv::c2 }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -11 }, { id::dlyOn, 1 },
          { id::dlyDiv, dv::d38 }, { id::dlyFb, 45 }, { id::dlyMix, 25 }, { id::arpOn, 1 },
          { id::arpMode, (float) arpDiverge }, { id::arpRate, dv::d16 }, { id::arpOct, 3 }, { id::arpGate, 60 },
          { id::arpSwing, 0 }, { id::arpLen, 10 }, { id::arpAcc, 50 } },
        { { srcLfo2, dstO1Pos, 35 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cArp, u8 ("Bit Saltador"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 60 }, { O (0, "warp"), wp::quantizar }, { O (0, "wamt"), 40 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 30 }, { E (0, "a"), 0.5f }, { E (0, "d"), 160 }, { E (0, "s"), 0 },
          { E (0, "r"), 80 }, { id::dstOn, 1 }, { id::dstType, ds::bitcrush }, { id::dstDrive, 30 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 30 }, { id::dlyMix, 15 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpRandom }, { id::arpRate, dv::d32 }, { id::arpOct, 3 },
          { id::arpGate, 30 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 }, { id::arpScale, 13 },
          { id::arpRoot, 4 }, { arpStepId (2), 0 }, { arpStepId (4), 0 }, { arpStepId (8), 0 }, { arpStepId (11), 0 },
          { arpStepId (13), 0 }, { arpAccId (0), 1 }, { arpAccId (3), 1 }, { arpAccId (7), 1 }, { arpAccId (10), 1 },
          { arpAccId (15), 1 } },
        { { srcRandom, dstO1Pos, 30 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Warp, 40 } }));

    p.push_back (mk (cArp, u8 ("Curto-Circuito"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 60 }, { O (0, "warp"), wp::quantizar }, { O (0, "wamt"), 70 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 30 }, { E (0, "a"), 0.5f }, { E (0, "d"), 160 }, { E (0, "s"), 0 },
          { E (0, "r"), 80 }, { id::dstOn, 1 }, { id::dstType, ds::bitcrush }, { id::dstDrive, 30 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 30 }, { id::dlyMix, 15 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpUp }, { id::arpRate, dv::d16t }, { id::arpOct, 4 },
          { id::arpGate, 25 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcRandom, dstO1Pos, 30 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Warp, 40 } }));

    p.push_back (mk (cArp, u8 ("Dado Viciado"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 60 }, { O (0, "warp"), wp::quantizar }, { O (0, "wamt"), 40 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 30 }, { E (0, "a"), 0.5f }, { E (0, "d"), 160 }, { E (0, "s"), 0 },
          { E (0, "r"), 80 }, { id::dstOn, 1 }, { id::dstType, ds::downsample }, { id::dstDrive, 30 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 30 }, { id::dlyMix, 15 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpConverge }, { id::arpRate, dv::d32 }, { id::arpOct, 2 },
          { id::arpGate, 40 }, { id::arpSwing, 0 }, { id::arpLen, 16 }, { id::arpAcc, 70 }, { arpAccId (0), 1 },
          { arpAccId (4), 1 }, { arpAccId (8), 1 }, { arpAccId (12), 1 } },
        { { srcRandom, dstO1Pos, 30 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Warp, 40 } }));

    p.push_back (mk (cArp, u8 ("Morse Quebrado"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 60 }, { O (0, "warp"), wp::quantizar }, { O (0, "wamt"), 40 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 55 }, { E (0, "a"), 0.5f }, { E (0, "d"), 160 }, { E (0, "s"), 0 },
          { E (0, "r"), 80 }, { id::dstOn, 1 }, { id::dstType, ds::bitcrush }, { id::dstDrive, 30 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 30 }, { id::dlyMix, 15 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpAsPlayed }, { id::arpRate, dv::d16 }, { id::arpOct, 1 },
          { id::arpGate, 45 }, { id::arpSwing, 0 }, { id::arpLen, 5 }, { id::arpAcc, 50 } },
        { { srcRandom, dstO1Pos, 30 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Warp, 40 } }));

    p.push_back (mk (cArp, u8 ("Fliperama"),
        { { O (0, "tab"), tb::eletrico }, { O (0, "pos"), 30 }, { O (0, "warp"), wp::quantizar }, { O (0, "wamt"), 40 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 30 }, { E (0, "a"), 0.5f }, { E (0, "d"), 160 }, { E (0, "s"), 0 },
          { E (0, "r"), 80 }, { id::dstOn, 1 }, { id::dstType, ds::bitcrush }, { id::dstDrive, 30 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 30 }, { id::dlyMix, 15 }, { id::volume, -12 },
          { id::arpOn, 1 }, { id::arpMode, (float) arpDown }, { id::arpRate, dv::d16 }, { id::arpOct, 3 },
          { id::arpGate, 50 }, { id::arpSwing, 50 }, { id::arpLen, 16 }, { id::arpAcc, 50 } },
        { { srcRandom, dstO1Pos, 30 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Warp, 40 } }));
