// Sprite, state, mobjtype and text identifiers
// Generated with DED Manager 1.1

#ifndef __INFO_CONSTANTS_H__
#define __INFO_CONSTANTS_H__

// Sprites.
typedef enum
{
	SPR_TROO,	// 000
	SPR_SHTG,	// 001
	SPR_PUNG,	// 002
	SPR_PISG,	// 003
	SPR_PISF,	// 004
	SPR_SHTF,	// 005
	SPR_SHT2,	// 006
	SPR_CHGG,	// 007
	SPR_CHGF,	// 008
	SPR_MISG,	// 009
	SPR_MISF,	// 010
	SPR_SAWG,	// 011
	SPR_PLSG,	// 012
	SPR_PLSF,	// 013
	SPR_BFGG,	// 014
	SPR_BFGF,	// 015
	SPR_BLUD,	// 016
	SPR_PUFF,	// 017
	SPR_BAL1,	// 018
	SPR_BAL2,	// 019
	SPR_PLSS,	// 020
	SPR_PLSE,	// 021
	SPR_MISL,	// 022
	SPR_BFS1,	// 023
	SPR_BFE1,	// 024
	SPR_BFE2,	// 025
	SPR_TFOG,	// 026
	SPR_IFOG,	// 027
	SPR_PLAY,	// 028
	SPR_POSS,	// 029
	SPR_SPOS,	// 030
	SPR_VILE,	// 031
	SPR_FIRE,	// 032
	SPR_FATB,	// 033
	SPR_FBXP,	// 034
	SPR_SKEL,	// 035
	SPR_MANF,	// 036
	SPR_FATT,	// 037
	SPR_CPOS,	// 038
	SPR_SARG,	// 039
	SPR_HEAD,	// 040
	SPR_BAL7,	// 041
	SPR_BOSS,	// 042
	SPR_BOS2,	// 043
	SPR_SKUL,	// 044
	SPR_SPID,	// 045
	SPR_BSPI,	// 046
	SPR_APLS,	// 047
	SPR_APBX,	// 048
	SPR_CYBR,	// 049
	SPR_PAIN,	// 050
	SPR_SSWV,	// 051
	SPR_KEEN,	// 052
	SPR_BBRN,	// 053
	SPR_BOSF,	// 054
	SPR_ARM1,	// 055
	SPR_ARM2,	// 056
	SPR_BAR1,	// 057
	SPR_BEXP,	// 058
	SPR_FCAN,	// 059
	SPR_BON1,	// 060
	SPR_BON2,	// 061
	SPR_BKEY,	// 062
	SPR_RKEY,	// 063
	SPR_YKEY,	// 064
	SPR_BSKU,	// 065
	SPR_RSKU,	// 066
	SPR_YSKU,	// 067
	SPR_STIM,	// 068
	SPR_MEDI,	// 069
	SPR_SOUL,	// 070
	SPR_PINV,	// 071
	SPR_PSTR,	// 072
	SPR_PINS,	// 073
	SPR_MEGA,	// 074
	SPR_SUIT,	// 075
	SPR_PMAP,	// 076
	SPR_PVIS,	// 077
	SPR_CLIP,	// 078
	SPR_AMMO,	// 079
	SPR_ROCK,	// 080
	SPR_BROK,	// 081
	SPR_CELL,	// 082
	SPR_CELP,	// 083
	SPR_SHEL,	// 084
	SPR_SBOX,	// 085
	SPR_BPAK,	// 086
	SPR_BFUG,	// 087
	SPR_MGUN,	// 088
	SPR_CSAW,	// 089
	SPR_LAUN,	// 090
	SPR_PLAS,	// 091
	SPR_SHOT,	// 092
	SPR_SGN2,	// 093
	SPR_COLU,	// 094
	SPR_SMT2,	// 095
	SPR_GOR1,	// 096
	SPR_POL2,	// 097
	SPR_POL5,	// 098
	SPR_POL4,	// 099
	SPR_POL3,	// 100
	SPR_POL1,	// 101
	SPR_POL6,	// 102
	SPR_GOR2,	// 103
	SPR_GOR3,	// 104
	SPR_GOR4,	// 105
	SPR_GOR5,	// 106
	SPR_SMIT,	// 107
	SPR_COL1,	// 108
	SPR_COL2,	// 109
	SPR_COL3,	// 110
	SPR_COL4,	// 111
	SPR_CAND,	// 112
	SPR_CBRA,	// 113
	SPR_COL6,	// 114
	SPR_TRE1,	// 115
	SPR_TRE2,	// 116
	SPR_ELEC,	// 117
	SPR_CEYE,	// 118
	SPR_FSKU,	// 119
	SPR_COL5,	// 120
	SPR_TBLU,	// 121
	SPR_TGRN,	// 122
	SPR_TRED,	// 123
	SPR_SMBT,	// 124
	SPR_SMGT,	// 125
	SPR_SMRT,	// 126
	SPR_HDB1,	// 127
	SPR_HDB2,	// 128
	SPR_HDB3,	// 129
	SPR_HDB4,	// 130
	SPR_HDB5,	// 131
	SPR_HDB6,	// 132
	SPR_POB1,	// 133
	SPR_POB2,	// 134
	SPR_BRS1,	// 135
	SPR_TLMP,	// 136
	SPR_TLP2,	// 137
	NUMSPRITES	// 138
} spritenum_t;

// States.
typedef enum
{
	S_NULL,	// 000
	S_LIGHTDONE,	// 001
	S_PUNCH,	// 002
	S_PUNCHDOWN,	// 003
	S_PUNCHUP,	// 004
	S_PUNCH1,	// 005
	S_PUNCH2,	// 006
	S_PUNCH3,	// 007
	S_PUNCH4,	// 008
	S_PUNCH5,	// 009
	S_PISTOL,	// 010
	S_PISTOLDOWN,	// 011
	S_PISTOLUP,	// 012
	S_PISTOL1,	// 013
	S_PISTOL2,	// 014
	S_PISTOL3,	// 015
	S_PISTOL4,	// 016
	S_PISTOLFLASH,	// 017
	S_SGUN,	// 018
	S_SGUNDOWN,	// 019
	S_SGUNUP,	// 020
	S_SGUN1,	// 021
	S_SGUN2,	// 022
	S_SGUN3,	// 023
	S_SGUN4,	// 024
	S_SGUN5,	// 025
	S_SGUN6,	// 026
	S_SGUN7,	// 027
	S_SGUN8,	// 028
	S_SGUN9,	// 029
	S_SGUNFLASH1,	// 030
	S_SGUNFLASH2,	// 031
	S_DSGUN,	// 032
	S_DSGUNDOWN,	// 033
	S_DSGUNUP,	// 034
	S_DSGUN1,	// 035
	S_DSGUN2,	// 036
	S_DSGUN3,	// 037
	S_DSGUN4,	// 038
	S_DSGUN5,	// 039
	S_DSGUN6,	// 040
	S_DSGUN7,	// 041
	S_DSGUN8,	// 042
	S_DSGUN9,	// 043
	S_DSGUN10,	// 044
	S_DSNR1,	// 045
	S_DSNR2,	// 046
	S_DSGUNFLASH1,	// 047
	S_DSGUNFLASH2,	// 048
	S_CHAIN,	// 049
	S_CHAINDOWN,	// 050
	S_CHAINUP,	// 051
	S_CHAIN1,	// 052
	S_CHAIN2,	// 053
	S_CHAIN3,	// 054
	S_CHAINFLASH1,	// 055
	S_CHAINFLASH2,	// 056
	S_MISSILE,	// 057
	S_MISSILEDOWN,	// 058
	S_MISSILEUP,	// 059
	S_MISSILE1,	// 060
	S_MISSILE2,	// 061
	S_MISSILE3,	// 062
	S_MISSILEFLASH1,	// 063
	S_MISSILEFLASH2,	// 064
	S_MISSILEFLASH3,	// 065
	S_MISSILEFLASH4,	// 066
	S_SAW,	// 067
	S_SAWB,	// 068
	S_SAWDOWN,	// 069
	S_SAWUP,	// 070
	S_SAW1,	// 071
	S_SAW2,	// 072
	S_SAW3,	// 073
	S_PLASMA,	// 074
	S_PLASMADOWN,	// 075
	S_PLASMAUP,	// 076
	S_PLASMA1,	// 077
	S_PLASMA2,	// 078
	S_PLASMAFLASH1,	// 079
	S_PLASMAFLASH2,	// 080
	S_BFG,	// 081
	S_BFGDOWN,	// 082
	S_BFGUP,	// 083
	S_BFG1,	// 084
	S_BFG2,	// 085
	S_BFG3,	// 086
	S_BFG4,	// 087
	S_BFGFLASH1,	// 088
	S_BFGFLASH2,	// 089
	S_BLOOD1,	// 090
	S_BLOOD2,	// 091
	S_BLOOD3,	// 092
	S_PUFF1,	// 093
	S_PUFF2,	// 094
	S_PUFF3,	// 095
	S_PUFF4,	// 096
	S_TBALL1,	// 097
	S_TBALL2,	// 098
	S_TBALLX1,	// 099
	S_TBALLX2,	// 100
	S_TBALLX3,	// 101
	S_RBALL1,	// 102
	S_RBALL2,	// 103
	S_RBALLX1,	// 104
	S_RBALLX2,	// 105
	S_RBALLX3,	// 106
	S_PLASBALL,	// 107
	S_PLASBALL2,	// 108
	S_PLASEXP,	// 109
	S_PLASEXP2,	// 110
	S_PLASEXP3,	// 111
	S_PLASEXP4,	// 112
	S_PLASEXP5,	// 113
	S_ROCKET,	// 114
	S_BFGSHOT,	// 115
	S_BFGSHOT2,	// 116
	S_BFGLAND,	// 117
	S_BFGLAND2,	// 118
	S_BFGLAND3,	// 119
	S_BFGLAND4,	// 120
	S_BFGLAND5,	// 121
	S_BFGLAND6,	// 122
	S_BFGEXP,	// 123
	S_BFGEXP2,	// 124
	S_BFGEXP3,	// 125
	S_BFGEXP4,	// 126
	S_EXPLODE1,	// 127
	S_EXPLODE2,	// 128
	S_EXPLODE3,	// 129
	S_TFOG,	// 130
	S_TFOG01,	// 131
	S_TFOG02,	// 132
	S_TFOG2,	// 133
	S_TFOG3,	// 134
	S_TFOG4,	// 135
	S_TFOG5,	// 136
	S_TFOG6,	// 137
	S_TFOG7,	// 138
	S_TFOG8,	// 139
	S_TFOG9,	// 140
	S_TFOG10,	// 141
	S_IFOG,	// 142
	S_IFOG01,	// 143
	S_IFOG02,	// 144
	S_IFOG2,	// 145
	S_IFOG3,	// 146
	S_IFOG4,	// 147
	S_IFOG5,	// 148
	S_PLAY,	// 149
	S_PLAY_RUN1,	// 150
	S_PLAY_RUN2,	// 151
	S_PLAY_RUN3,	// 152
	S_PLAY_RUN4,	// 153
	S_PLAY_ATK1,	// 154
	S_PLAY_ATK2,	// 155
	S_PLAY_PAIN,	// 156
	S_PLAY_PAIN2,	// 157
	S_PLAY_DIE1,	// 158
	S_PLAY_DIE2,	// 159
	S_PLAY_DIE3,	// 160
	S_PLAY_DIE4,	// 161
	S_PLAY_DIE5,	// 162
	S_PLAY_DIE6,	// 163
	S_PLAY_DIE7,	// 164
	S_PLAY_XDIE1,	// 165
	S_PLAY_XDIE2,	// 166
	S_PLAY_XDIE3,	// 167
	S_PLAY_XDIE4,	// 168
	S_PLAY_XDIE5,	// 169
	S_PLAY_XDIE6,	// 170
	S_PLAY_XDIE7,	// 171
	S_PLAY_XDIE8,	// 172
	S_PLAY_XDIE9,	// 173
	S_POSS_STND,	// 174
	S_POSS_STND2,	// 175
	S_POSS_RUN1,	// 176
	S_POSS_RUN2,	// 177
	S_POSS_RUN3,	// 178
	S_POSS_RUN4,	// 179
	S_POSS_RUN5,	// 180
	S_POSS_RUN6,	// 181
	S_POSS_RUN7,	// 182
	S_POSS_RUN8,	// 183
	S_POSS_ATK1,	// 184
	S_POSS_ATK2,	// 185
	S_POSS_ATK3,	// 186
	S_POSS_PAIN,	// 187
	S_POSS_PAIN2,	// 188
	S_POSS_DIE1,	// 189
	S_POSS_DIE2,	// 190
	S_POSS_DIE3,	// 191
	S_POSS_DIE4,	// 192
	S_POSS_DIE5,	// 193
	S_POSS_XDIE1,	// 194
	S_POSS_XDIE2,	// 195
	S_POSS_XDIE3,	// 196
	S_POSS_XDIE4,	// 197
	S_POSS_XDIE5,	// 198
	S_POSS_XDIE6,	// 199
	S_POSS_XDIE7,	// 200
	S_POSS_XDIE8,	// 201
	S_POSS_XDIE9,	// 202
	S_POSS_RAISE1,	// 203
	S_POSS_RAISE2,	// 204
	S_POSS_RAISE3,	// 205
	S_POSS_RAISE4,	// 206
	S_SPOS_STND,	// 207
	S_SPOS_STND2,	// 208
	S_SPOS_RUN1,	// 209
	S_SPOS_RUN2,	// 210
	S_SPOS_RUN3,	// 211
	S_SPOS_RUN4,	// 212
	S_SPOS_RUN5,	// 213
	S_SPOS_RUN6,	// 214
	S_SPOS_RUN7,	// 215
	S_SPOS_RUN8,	// 216
	S_SPOS_ATK1,	// 217
	S_SPOS_ATK2,	// 218
	S_SPOS_ATK3,	// 219
	S_SPOS_PAIN,	// 220
	S_SPOS_PAIN2,	// 221
	S_SPOS_DIE1,	// 222
	S_SPOS_DIE2,	// 223
	S_SPOS_DIE3,	// 224
	S_SPOS_DIE4,	// 225
	S_SPOS_DIE5,	// 226
	S_SPOS_XDIE1,	// 227
	S_SPOS_XDIE2,	// 228
	S_SPOS_XDIE3,	// 229
	S_SPOS_XDIE4,	// 230
	S_SPOS_XDIE5,	// 231
	S_SPOS_XDIE6,	// 232
	S_SPOS_XDIE7,	// 233
	S_SPOS_XDIE8,	// 234
	S_SPOS_XDIE9,	// 235
	S_SPOS_RAISE1,	// 236
	S_SPOS_RAISE2,	// 237
	S_SPOS_RAISE3,	// 238
	S_SPOS_RAISE4,	// 239
	S_SPOS_RAISE5,	// 240
	S_VILE_STND,	// 241
	S_VILE_STND2,	// 242
	S_VILE_RUN1,	// 243
	S_VILE_RUN2,	// 244
	S_VILE_RUN3,	// 245
	S_VILE_RUN4,	// 246
	S_VILE_RUN5,	// 247
	S_VILE_RUN6,	// 248
	S_VILE_RUN7,	// 249
	S_VILE_RUN8,	// 250
	S_VILE_RUN9,	// 251
	S_VILE_RUN10,	// 252
	S_VILE_RUN11,	// 253
	S_VILE_RUN12,	// 254
	S_VILE_ATK1,	// 255
	S_VILE_ATK2,	// 256
	S_VILE_ATK3,	// 257
	S_VILE_ATK4,	// 258
	S_VILE_ATK5,	// 259
	S_VILE_ATK6,	// 260
	S_VILE_ATK7,	// 261
	S_VILE_ATK8,	// 262
	S_VILE_ATK9,	// 263
	S_VILE_ATK10,	// 264
	S_VILE_ATK11,	// 265
	S_VILE_HEAL1,	// 266
	S_VILE_HEAL2,	// 267
	S_VILE_HEAL3,	// 268
	S_VILE_PAIN,	// 269
	S_VILE_PAIN2,	// 270
	S_VILE_DIE1,	// 271
	S_VILE_DIE2,	// 272
	S_VILE_DIE3,	// 273
	S_VILE_DIE4,	// 274
	S_VILE_DIE5,	// 275
	S_VILE_DIE6,	// 276
	S_VILE_DIE7,	// 277
	S_VILE_DIE8,	// 278
	S_VILE_DIE9,	// 279
	S_VILE_DIE10,	// 280
	S_FIRE1,	// 281
	S_FIRE2,	// 282
	S_FIRE3,	// 283
	S_FIRE4,	// 284
	S_FIRE5,	// 285
	S_FIRE6,	// 286
	S_FIRE7,	// 287
	S_FIRE8,	// 288
	S_FIRE9,	// 289
	S_FIRE10,	// 290
	S_FIRE11,	// 291
	S_FIRE12,	// 292
	S_FIRE13,	// 293
	S_FIRE14,	// 294
	S_FIRE15,	// 295
	S_FIRE16,	// 296
	S_FIRE17,	// 297
	S_FIRE18,	// 298
	S_FIRE19,	// 299
	S_FIRE20,	// 300
	S_FIRE21,	// 301
	S_FIRE22,	// 302
	S_FIRE23,	// 303
	S_FIRE24,	// 304
	S_FIRE25,	// 305
	S_FIRE26,	// 306
	S_FIRE27,	// 307
	S_FIRE28,	// 308
	S_FIRE29,	// 309
	S_FIRE30,	// 310
	S_SMOKE1,	// 311
	S_SMOKE2,	// 312
	S_SMOKE3,	// 313
	S_SMOKE4,	// 314
	S_SMOKE5,	// 315
	S_TRACER,	// 316
	S_TRACER2,	// 317
	S_TRACEEXP1,	// 318
	S_TRACEEXP2,	// 319
	S_TRACEEXP3,	// 320
	S_SKEL_STND,	// 321
	S_SKEL_STND2,	// 322
	S_SKEL_RUN1,	// 323
	S_SKEL_RUN2,	// 324
	S_SKEL_RUN3,	// 325
	S_SKEL_RUN4,	// 326
	S_SKEL_RUN5,	// 327
	S_SKEL_RUN6,	// 328
	S_SKEL_RUN7,	// 329
	S_SKEL_RUN8,	// 330
	S_SKEL_RUN9,	// 331
	S_SKEL_RUN10,	// 332
	S_SKEL_RUN11,	// 333
	S_SKEL_RUN12,	// 334
	S_SKEL_FIST1,	// 335
	S_SKEL_FIST2,	// 336
	S_SKEL_FIST3,	// 337
	S_SKEL_FIST4,	// 338
	S_SKEL_MISS1,	// 339
	S_SKEL_MISS2,	// 340
	S_SKEL_MISS3,	// 341
	S_SKEL_MISS4,	// 342
	S_SKEL_PAIN,	// 343
	S_SKEL_PAIN2,	// 344
	S_SKEL_DIE1,	// 345
	S_SKEL_DIE2,	// 346
	S_SKEL_DIE3,	// 347
	S_SKEL_DIE4,	// 348
	S_SKEL_DIE5,	// 349
	S_SKEL_DIE6,	// 350
	S_SKEL_RAISE1,	// 351
	S_SKEL_RAISE2,	// 352
	S_SKEL_RAISE3,	// 353
	S_SKEL_RAISE4,	// 354
	S_SKEL_RAISE5,	// 355
	S_SKEL_RAISE6,	// 356
	S_FATSHOT1,	// 357
	S_FATSHOT2,	// 358
	S_FATSHOTX1,	// 359
	S_FATSHOTX2,	// 360
	S_FATSHOTX3,	// 361
	S_FATT_STND,	// 362
	S_FATT_STND2,	// 363
	S_FATT_RUN1,	// 364
	S_FATT_RUN2,	// 365
	S_FATT_RUN3,	// 366
	S_FATT_RUN4,	// 367
	S_FATT_RUN5,	// 368
	S_FATT_RUN6,	// 369
	S_FATT_RUN7,	// 370
	S_FATT_RUN8,	// 371
	S_FATT_RUN9,	// 372
	S_FATT_RUN10,	// 373
	S_FATT_RUN11,	// 374
	S_FATT_RUN12,	// 375
	S_FATT_ATK1,	// 376
	S_FATT_ATK2,	// 377
	S_FATT_ATK3,	// 378
	S_FATT_ATK4,	// 379
	S_FATT_ATK5,	// 380
	S_FATT_ATK6,	// 381
	S_FATT_ATK7,	// 382
	S_FATT_ATK8,	// 383
	S_FATT_ATK9,	// 384
	S_FATT_ATK10,	// 385
	S_FATT_PAIN,	// 386
	S_FATT_PAIN2,	// 387
	S_FATT_DIE1,	// 388
	S_FATT_DIE2,	// 389
	S_FATT_DIE3,	// 390
	S_FATT_DIE4,	// 391
	S_FATT_DIE5,	// 392
	S_FATT_DIE6,	// 393
	S_FATT_DIE7,	// 394
	S_FATT_DIE8,	// 395
	S_FATT_DIE9,	// 396
	S_FATT_DIE10,	// 397
	S_FATT_RAISE1,	// 398
	S_FATT_RAISE2,	// 399
	S_FATT_RAISE3,	// 400
	S_FATT_RAISE4,	// 401
	S_FATT_RAISE5,	// 402
	S_FATT_RAISE6,	// 403
	S_FATT_RAISE7,	// 404
	S_FATT_RAISE8,	// 405
	S_CPOS_STND,	// 406
	S_CPOS_STND2,	// 407
	S_CPOS_RUN1,	// 408
	S_CPOS_RUN2,	// 409
	S_CPOS_RUN3,	// 410
	S_CPOS_RUN4,	// 411
	S_CPOS_RUN5,	// 412
	S_CPOS_RUN6,	// 413
	S_CPOS_RUN7,	// 414
	S_CPOS_RUN8,	// 415
	S_CPOS_ATK1,	// 416
	S_CPOS_ATK2,	// 417
	S_CPOS_ATK3,	// 418
	S_CPOS_ATK4,	// 419
	S_CPOS_PAIN,	// 420
	S_CPOS_PAIN2,	// 421
	S_CPOS_DIE1,	// 422
	S_CPOS_DIE2,	// 423
	S_CPOS_DIE3,	// 424
	S_CPOS_DIE4,	// 425
	S_CPOS_DIE5,	// 426
	S_CPOS_DIE6,	// 427
	S_CPOS_DIE7,	// 428
	S_CPOS_XDIE1,	// 429
	S_CPOS_XDIE2,	// 430
	S_CPOS_XDIE3,	// 431
	S_CPOS_XDIE4,	// 432
	S_CPOS_XDIE5,	// 433
	S_CPOS_XDIE6,	// 434
	S_CPOS_RAISE1,	// 435
	S_CPOS_RAISE2,	// 436
	S_CPOS_RAISE3,	// 437
	S_CPOS_RAISE4,	// 438
	S_CPOS_RAISE5,	// 439
	S_CPOS_RAISE6,	// 440
	S_CPOS_RAISE7,	// 441
	S_TROO_STND,	// 442
	S_TROO_STND2,	// 443
	S_TROO_RUN1,	// 444
	S_TROO_RUN2,	// 445
	S_TROO_RUN3,	// 446
	S_TROO_RUN4,	// 447
	S_TROO_RUN5,	// 448
	S_TROO_RUN6,	// 449
	S_TROO_RUN7,	// 450
	S_TROO_RUN8,	// 451
	S_TROO_ATK1,	// 452
	S_TROO_ATK2,	// 453
	S_TROO_ATK3,	// 454
	S_TROO_PAIN,	// 455
	S_TROO_PAIN2,	// 456
	S_TROO_DIE1,	// 457
	S_TROO_DIE2,	// 458
	S_TROO_DIE3,	// 459
	S_TROO_DIE4,	// 460
	S_TROO_DIE5,	// 461
	S_TROO_XDIE1,	// 462
	S_TROO_XDIE2,	// 463
	S_TROO_XDIE3,	// 464
	S_TROO_XDIE4,	// 465
	S_TROO_XDIE5,	// 466
	S_TROO_XDIE6,	// 467
	S_TROO_XDIE7,	// 468
	S_TROO_XDIE8,	// 469
	S_TROO_RAISE1,	// 470
	S_TROO_RAISE2,	// 471
	S_TROO_RAISE3,	// 472
	S_TROO_RAISE4,	// 473
	S_TROO_RAISE5,	// 474
	S_SARG_STND,	// 475
	S_SARG_STND2,	// 476
	S_SARG_RUN1,	// 477
	S_SARG_RUN2,	// 478
	S_SARG_RUN3,	// 479
	S_SARG_RUN4,	// 480
	S_SARG_RUN5,	// 481
	S_SARG_RUN6,	// 482
	S_SARG_RUN7,	// 483
	S_SARG_RUN8,	// 484
	S_SARG_ATK1,	// 485
	S_SARG_ATK2,	// 486
	S_SARG_ATK3,	// 487
	S_SARG_PAIN,	// 488
	S_SARG_PAIN2,	// 489
	S_SARG_DIE1,	// 490
	S_SARG_DIE2,	// 491
	S_SARG_DIE3,	// 492
	S_SARG_DIE4,	// 493
	S_SARG_DIE5,	// 494
	S_SARG_DIE6,	// 495
	S_SARG_RAISE1,	// 496
	S_SARG_RAISE2,	// 497
	S_SARG_RAISE3,	// 498
	S_SARG_RAISE4,	// 499
	S_SARG_RAISE5,	// 500
	S_SARG_RAISE6,	// 501
	S_HEAD_STND,	// 502
	S_HEAD_RUN1,	// 503
	S_HEAD_ATK1,	// 504
	S_HEAD_ATK2,	// 505
	S_HEAD_ATK3,	// 506
	S_HEAD_PAIN,	// 507
	S_HEAD_PAIN2,	// 508
	S_HEAD_PAIN3,	// 509
	S_HEAD_DIE1,	// 510
	S_HEAD_DIE2,	// 511
	S_HEAD_DIE3,	// 512
	S_HEAD_DIE4,	// 513
	S_HEAD_DIE5,	// 514
	S_HEAD_DIE6,	// 515
	S_HEAD_RAISE1,	// 516
	S_HEAD_RAISE2,	// 517
	S_HEAD_RAISE3,	// 518
	S_HEAD_RAISE4,	// 519
	S_HEAD_RAISE5,	// 520
	S_HEAD_RAISE6,	// 521
	S_BRBALL1,	// 522
	S_BRBALL2,	// 523
	S_BRBALLX1,	// 524
	S_BRBALLX2,	// 525
	S_BRBALLX3,	// 526
	S_BOSS_STND,	// 527
	S_BOSS_STND2,	// 528
	S_BOSS_RUN1,	// 529
	S_BOSS_RUN2,	// 530
	S_BOSS_RUN3,	// 531
	S_BOSS_RUN4,	// 532
	S_BOSS_RUN5,	// 533
	S_BOSS_RUN6,	// 534
	S_BOSS_RUN7,	// 535
	S_BOSS_RUN8,	// 536
	S_BOSS_ATK1,	// 537
	S_BOSS_ATK2,	// 538
	S_BOSS_ATK3,	// 539
	S_BOSS_PAIN,	// 540
	S_BOSS_PAIN2,	// 541
	S_BOSS_DIE1,	// 542
	S_BOSS_DIE2,	// 543
	S_BOSS_DIE3,	// 544
	S_BOSS_DIE4,	// 545
	S_BOSS_DIE5,	// 546
	S_BOSS_DIE6,	// 547
	S_BOSS_DIE7,	// 548
	S_BOSS_RAISE1,	// 549
	S_BOSS_RAISE2,	// 550
	S_BOSS_RAISE3,	// 551
	S_BOSS_RAISE4,	// 552
	S_BOSS_RAISE5,	// 553
	S_BOSS_RAISE6,	// 554
	S_BOSS_RAISE7,	// 555
	S_BOS2_STND,	// 556
	S_BOS2_STND2,	// 557
	S_BOS2_RUN1,	// 558
	S_BOS2_RUN2,	// 559
	S_BOS2_RUN3,	// 560
	S_BOS2_RUN4,	// 561
	S_BOS2_RUN5,	// 562
	S_BOS2_RUN6,	// 563
	S_BOS2_RUN7,	// 564
	S_BOS2_RUN8,	// 565
	S_BOS2_ATK1,	// 566
	S_BOS2_ATK2,	// 567
	S_BOS2_ATK3,	// 568
	S_BOS2_PAIN,	// 569
	S_BOS2_PAIN2,	// 570
	S_BOS2_DIE1,	// 571
	S_BOS2_DIE2,	// 572
	S_BOS2_DIE3,	// 573
	S_BOS2_DIE4,	// 574
	S_BOS2_DIE5,	// 575
	S_BOS2_DIE6,	// 576
	S_BOS2_DIE7,	// 577
	S_BOS2_RAISE1,	// 578
	S_BOS2_RAISE2,	// 579
	S_BOS2_RAISE3,	// 580
	S_BOS2_RAISE4,	// 581
	S_BOS2_RAISE5,	// 582
	S_BOS2_RAISE6,	// 583
	S_BOS2_RAISE7,	// 584
	S_SKULL_STND,	// 585
	S_SKULL_STND2,	// 586
	S_SKULL_RUN1,	// 587
	S_SKULL_RUN2,	// 588
	S_SKULL_ATK1,	// 589
	S_SKULL_ATK2,	// 590
	S_SKULL_ATK3,	// 591
	S_SKULL_ATK4,	// 592
	S_SKULL_PAIN,	// 593
	S_SKULL_PAIN2,	// 594
	S_SKULL_DIE1,	// 595
	S_SKULL_DIE2,	// 596
	S_SKULL_DIE3,	// 597
	S_SKULL_DIE4,	// 598
	S_SKULL_DIE5,	// 599
	S_SKULL_DIE6,	// 600
	S_SPID_STND,	// 601
	S_SPID_STND2,	// 602
	S_SPID_RUN1,	// 603
	S_SPID_RUN2,	// 604
	S_SPID_RUN3,	// 605
	S_SPID_RUN4,	// 606
	S_SPID_RUN5,	// 607
	S_SPID_RUN6,	// 608
	S_SPID_RUN7,	// 609
	S_SPID_RUN8,	// 610
	S_SPID_RUN9,	// 611
	S_SPID_RUN10,	// 612
	S_SPID_RUN11,	// 613
	S_SPID_RUN12,	// 614
	S_SPID_ATK1,	// 615
	S_SPID_ATK2,	// 616
	S_SPID_ATK3,	// 617
	S_SPID_ATK4,	// 618
	S_SPID_PAIN,	// 619
	S_SPID_PAIN2,	// 620
	S_SPID_DIE1,	// 621
	S_SPID_DIE2,	// 622
	S_SPID_DIE3,	// 623
	S_SPID_DIE4,	// 624
	S_SPID_DIE5,	// 625
	S_SPID_DIE6,	// 626
	S_SPID_DIE7,	// 627
	S_SPID_DIE8,	// 628
	S_SPID_DIE9,	// 629
	S_SPID_DIE10,	// 630
	S_SPID_DIE11,	// 631
	S_BSPI_STND,	// 632
	S_BSPI_STND2,	// 633
	S_BSPI_SIGHT,	// 634
	S_BSPI_RUN1,	// 635
	S_BSPI_RUN2,	// 636
	S_BSPI_RUN3,	// 637
	S_BSPI_RUN4,	// 638
	S_BSPI_RUN5,	// 639
	S_BSPI_RUN6,	// 640
	S_BSPI_RUN7,	// 641
	S_BSPI_RUN8,	// 642
	S_BSPI_RUN9,	// 643
	S_BSPI_RUN10,	// 644
	S_BSPI_RUN11,	// 645
	S_BSPI_RUN12,	// 646
	S_BSPI_ATK1,	// 647
	S_BSPI_ATK2,	// 648
	S_BSPI_ATK3,	// 649
	S_BSPI_ATK4,	// 650
	S_BSPI_PAIN,	// 651
	S_BSPI_PAIN2,	// 652
	S_BSPI_DIE1,	// 653
	S_BSPI_DIE2,	// 654
	S_BSPI_DIE3,	// 655
	S_BSPI_DIE4,	// 656
	S_BSPI_DIE5,	// 657
	S_BSPI_DIE6,	// 658
	S_BSPI_DIE7,	// 659
	S_BSPI_RAISE1,	// 660
	S_BSPI_RAISE2,	// 661
	S_BSPI_RAISE3,	// 662
	S_BSPI_RAISE4,	// 663
	S_BSPI_RAISE5,	// 664
	S_BSPI_RAISE6,	// 665
	S_BSPI_RAISE7,	// 666
	S_ARACH_PLAZ,	// 667
	S_ARACH_PLAZ2,	// 668
	S_ARACH_PLEX,	// 669
	S_ARACH_PLEX2,	// 670
	S_ARACH_PLEX3,	// 671
	S_ARACH_PLEX4,	// 672
	S_ARACH_PLEX5,	// 673
	S_CYBER_STND,	// 674
	S_CYBER_STND2,	// 675
	S_CYBER_RUN1,	// 676
	S_CYBER_RUN2,	// 677
	S_CYBER_RUN3,	// 678
	S_CYBER_RUN4,	// 679
	S_CYBER_RUN5,	// 680
	S_CYBER_RUN6,	// 681
	S_CYBER_RUN7,	// 682
	S_CYBER_RUN8,	// 683
	S_CYBER_ATK1,	// 684
	S_CYBER_ATK2,	// 685
	S_CYBER_ATK3,	// 686
	S_CYBER_ATK4,	// 687
	S_CYBER_ATK5,	// 688
	S_CYBER_ATK6,	// 689
	S_CYBER_PAIN,	// 690
	S_CYBER_DIE1,	// 691
	S_CYBER_DIE2,	// 692
	S_CYBER_DIE3,	// 693
	S_CYBER_DIE4,	// 694
	S_CYBER_DIE5,	// 695
	S_CYBER_DIE6,	// 696
	S_CYBER_DIE7,	// 697
	S_CYBER_DIE8,	// 698
	S_CYBER_DIE9,	// 699
	S_CYBER_DIE10,	// 700
	S_PAIN_STND,	// 701
	S_PAIN_RUN1,	// 702
	S_PAIN_RUN2,	// 703
	S_PAIN_RUN3,	// 704
	S_PAIN_RUN4,	// 705
	S_PAIN_RUN5,	// 706
	S_PAIN_RUN6,	// 707
	S_PAIN_ATK1,	// 708
	S_PAIN_ATK2,	// 709
	S_PAIN_ATK3,	// 710
	S_PAIN_ATK4,	// 711
	S_PAIN_PAIN,	// 712
	S_PAIN_PAIN2,	// 713
	S_PAIN_DIE1,	// 714
	S_PAIN_DIE2,	// 715
	S_PAIN_DIE3,	// 716
	S_PAIN_DIE4,	// 717
	S_PAIN_DIE5,	// 718
	S_PAIN_DIE6,	// 719
	S_PAIN_RAISE1,	// 720
	S_PAIN_RAISE2,	// 721
	S_PAIN_RAISE3,	// 722
	S_PAIN_RAISE4,	// 723
	S_PAIN_RAISE5,	// 724
	S_PAIN_RAISE6,	// 725
	S_SSWV_STND,	// 726
	S_SSWV_STND2,	// 727
	S_SSWV_RUN1,	// 728
	S_SSWV_RUN2,	// 729
	S_SSWV_RUN3,	// 730
	S_SSWV_RUN4,	// 731
	S_SSWV_RUN5,	// 732
	S_SSWV_RUN6,	// 733
	S_SSWV_RUN7,	// 734
	S_SSWV_RUN8,	// 735
	S_SSWV_ATK1,	// 736
	S_SSWV_ATK2,	// 737
	S_SSWV_ATK3,	// 738
	S_SSWV_ATK4,	// 739
	S_SSWV_ATK5,	// 740
	S_SSWV_ATK6,	// 741
	S_SSWV_PAIN,	// 742
	S_SSWV_PAIN2,	// 743
	S_SSWV_DIE1,	// 744
	S_SSWV_DIE2,	// 745
	S_SSWV_DIE3,	// 746
	S_SSWV_DIE4,	// 747
	S_SSWV_DIE5,	// 748
	S_SSWV_XDIE1,	// 749
	S_SSWV_XDIE2,	// 750
	S_SSWV_XDIE3,	// 751
	S_SSWV_XDIE4,	// 752
	S_SSWV_XDIE5,	// 753
	S_SSWV_XDIE6,	// 754
	S_SSWV_XDIE7,	// 755
	S_SSWV_XDIE8,	// 756
	S_SSWV_XDIE9,	// 757
	S_SSWV_RAISE1,	// 758
	S_SSWV_RAISE2,	// 759
	S_SSWV_RAISE3,	// 760
	S_SSWV_RAISE4,	// 761
	S_SSWV_RAISE5,	// 762
	S_KEENSTND,	// 763
	S_COMMKEEN,	// 764
	S_COMMKEEN2,	// 765
	S_COMMKEEN3,	// 766
	S_COMMKEEN4,	// 767
	S_COMMKEEN5,	// 768
	S_COMMKEEN6,	// 769
	S_COMMKEEN7,	// 770
	S_COMMKEEN8,	// 771
	S_COMMKEEN9,	// 772
	S_COMMKEEN10,	// 773
	S_COMMKEEN11,	// 774
	S_COMMKEEN12,	// 775
	S_KEENPAIN,	// 776
	S_KEENPAIN2,	// 777
	S_BRAIN,	// 778
	S_BRAIN_PAIN,	// 779
	S_BRAIN_DIE1,	// 780
	S_BRAIN_DIE2,	// 781
	S_BRAIN_DIE3,	// 782
	S_BRAIN_DIE4,	// 783
	S_BRAINEYE,	// 784
	S_BRAINEYESEE,	// 785
	S_BRAINEYE1,	// 786
	S_SPAWN1,	// 787
	S_SPAWN2,	// 788
	S_SPAWN3,	// 789
	S_SPAWN4,	// 790
	S_SPAWNFIRE1,	// 791
	S_SPAWNFIRE2,	// 792
	S_SPAWNFIRE3,	// 793
	S_SPAWNFIRE4,	// 794
	S_SPAWNFIRE5,	// 795
	S_SPAWNFIRE6,	// 796
	S_SPAWNFIRE7,	// 797
	S_SPAWNFIRE8,	// 798
	S_BRAINEXPLODE1,	// 799
	S_BRAINEXPLODE2,	// 800
	S_BRAINEXPLODE3,	// 801
	S_ARM1,	// 802
	S_ARM1A,	// 803
	S_ARM2,	// 804
	S_ARM2A,	// 805
	S_BAR1,	// 806
	S_BAR2,	// 807
	S_BEXP,	// 808
	S_BEXP2,	// 809
	S_BEXP3,	// 810
	S_BEXP4,	// 811
	S_BEXP5,	// 812
	S_BBAR1,	// 813
	S_BBAR2,	// 814
	S_BBAR3,	// 815
	S_BON1,	// 816
	S_BON1A,	// 817
	S_BON1B,	// 818
	S_BON1C,	// 819
	S_BON1D,	// 820
	S_BON1E,	// 821
	S_BON2,	// 822
	S_BON2A,	// 823
	S_BON2B,	// 824
	S_BON2C,	// 825
	S_BON2D,	// 826
	S_BON2E,	// 827
	S_BKEY,	// 828
	S_BKEY2,	// 829
	S_RKEY,	// 830
	S_RKEY2,	// 831
	S_YKEY,	// 832
	S_YKEY2,	// 833
	S_BSKULL,	// 834
	S_BSKULL2,	// 835
	S_RSKULL,	// 836
	S_RSKULL2,	// 837
	S_YSKULL,	// 838
	S_YSKULL2,	// 839
	S_STIM,	// 840
	S_MEDI,	// 841
	S_SOUL,	// 842
	S_SOUL2,	// 843
	S_SOUL3,	// 844
	S_SOUL4,	// 845
	S_SOUL5,	// 846
	S_SOUL6,	// 847
	S_PINV,	// 848
	S_PINV2,	// 849
	S_PINV3,	// 850
	S_PINV4,	// 851
	S_PSTR,	// 852
	S_PINS,	// 853
	S_PINS2,	// 854
	S_PINS3,	// 855
	S_PINS4,	// 856
	S_MEGA,	// 857
	S_MEGA2,	// 858
	S_MEGA3,	// 859
	S_MEGA4,	// 860
	S_SUIT,	// 861
	S_PMAP,	// 862
	S_PMAP2,	// 863
	S_PMAP3,	// 864
	S_PMAP4,	// 865
	S_PMAP5,	// 866
	S_PMAP6,	// 867
	S_PVIS,	// 868
	S_PVIS2,	// 869
	S_CLIP,	// 870
	S_AMMO,	// 871
	S_ROCK,	// 872
	S_BROK,	// 873
	S_CELL,	// 874
	S_CELP,	// 875
	S_SHEL,	// 876
	S_SBOX,	// 877
	S_BPAK,	// 878
	S_BFUG,	// 879
	S_MGUN,	// 880
	S_CSAW,	// 881
	S_LAUN,	// 882
	S_PLAS,	// 883
	S_SHOT,	// 884
	S_SHOT2,	// 885
	S_COLU,	// 886
	S_STALAG,	// 887
	S_BLOODYTWITCH,	// 888
	S_BLOODYTWITCH2,	// 889
	S_BLOODYTWITCH3,	// 890
	S_BLOODYTWITCH4,	// 891
	S_DEADTORSO,	// 892
	S_DEADBOTTOM,	// 893
	S_HEADSONSTICK,	// 894
	S_GIBS,	// 895
	S_HEADONASTICK,	// 896
	S_HEADCANDLES,	// 897
	S_HEADCANDLES2,	// 898
	S_DEADSTICK,	// 899
	S_LIVESTICK,	// 900
	S_LIVESTICK2,	// 901
	S_MEAT2,	// 902
	S_MEAT3,	// 903
	S_MEAT4,	// 904
	S_MEAT5,	// 905
	S_STALAGTITE,	// 906
	S_TALLGRNCOL,	// 907
	S_SHRTGRNCOL,	// 908
	S_TALLREDCOL,	// 909
	S_SHRTREDCOL,	// 910
	S_CANDLESTIK,	// 911
	S_CANDELABRA,	// 912
	S_SKULLCOL,	// 913
	S_TORCHTREE,	// 914
	S_BIGTREE,	// 915
	S_TECHPILLAR,	// 916
	S_EVILEYE,	// 917
	S_EVILEYE2,	// 918
	S_EVILEYE3,	// 919
	S_EVILEYE4,	// 920
	S_FLOATSKULL,	// 921
	S_FLOATSKULL2,	// 922
	S_FLOATSKULL3,	// 923
	S_HEARTCOL,	// 924
	S_HEARTCOL2,	// 925
	S_BLUETORCH,	// 926
	S_BLUETORCH2,	// 927
	S_BLUETORCH3,	// 928
	S_BLUETORCH4,	// 929
	S_GREENTORCH,	// 930
	S_GREENTORCH2,	// 931
	S_GREENTORCH3,	// 932
	S_GREENTORCH4,	// 933
	S_REDTORCH,	// 934
	S_REDTORCH2,	// 935
	S_REDTORCH3,	// 936
	S_REDTORCH4,	// 937
	S_BTORCHSHRT,	// 938
	S_BTORCHSHRT2,	// 939
	S_BTORCHSHRT3,	// 940
	S_BTORCHSHRT4,	// 941
	S_GTORCHSHRT,	// 942
	S_GTORCHSHRT2,	// 943
	S_GTORCHSHRT3,	// 944
	S_GTORCHSHRT4,	// 945
	S_RTORCHSHRT,	// 946
	S_RTORCHSHRT2,	// 947
	S_RTORCHSHRT3,	// 948
	S_RTORCHSHRT4,	// 949
	S_HANGNOGUTS,	// 950
	S_HANGBNOBRAIN,	// 951
	S_HANGTLOOKDN,	// 952
	S_HANGTSKULL,	// 953
	S_HANGTLOOKUP,	// 954
	S_HANGTNOBRAIN,	// 955
	S_COLONGIBS,	// 956
	S_SMALLPOOL,	// 957
	S_BRAINSTEM,	// 958
	S_TECHLAMP,	// 959
	S_TECHLAMP2,	// 960
	S_TECHLAMP3,	// 961
	S_TECHLAMP4,	// 962
	S_TECH2LAMP,	// 963
	S_TECH2LAMP2,	// 964
	S_TECH2LAMP3,	// 965
	S_TECH2LAMP4,	// 966
	S_SMALL_WHITE_LIGHT,	// 967
	S_TEMPSOUNDORIGIN1,	// 968
	NUMSTATES	// 969
} statenum_t;

// Map objects.
typedef enum
{
	MT_PLAYER,	// 000
	MT_POSSESSED,	// 001
	MT_SHOTGUY,	// 002
	MT_VILE,	// 003
	MT_FIRE,	// 004
	MT_UNDEAD,	// 005
	MT_TRACER,	// 006
	MT_SMOKE,	// 007
	MT_FATSO,	// 008
	MT_FATSHOT,	// 009
	MT_CHAINGUY,	// 010
	MT_TROOP,	// 011
	MT_SERGEANT,	// 012
	MT_SHADOWS,	// 013
	MT_HEAD,	// 014
	MT_BRUISER,	// 015
	MT_BRUISERSHOT,	// 016
	MT_KNIGHT,	// 017
	MT_SKULL,	// 018
	MT_SPIDER,	// 019
	MT_BABY,	// 020
	MT_CYBORG,	// 021
	MT_PAIN,	// 022
	MT_WOLFSS,	// 023
	MT_KEEN,	// 024
	MT_BOSSBRAIN,	// 025
	MT_BOSSSPIT,	// 026
	MT_BOSSTARGET,	// 027
	MT_SPAWNSHOT,	// 028
	MT_SPAWNFIRE,	// 029
	MT_BARREL,	// 030
	MT_TROOPSHOT,	// 031
	MT_HEADSHOT,	// 032
	MT_ROCKET,	// 033
	MT_PLASMA,	// 034
	MT_BFG,	// 035
	MT_ARACHPLAZ,	// 036
	MT_PUFF,	// 037
	MT_BLOOD,	// 038
	MT_TFOG,	// 039
	MT_IFOG,	// 040
	MT_TELEPORTMAN,	// 041
	MT_EXTRABFG,	// 042
	MT_MISC0,	// 043
	MT_MISC1,	// 044
	MT_MISC2,	// 045
	MT_MISC3,	// 046
	MT_MISC4,	// 047
	MT_MISC5,	// 048
	MT_MISC6,	// 049
	MT_MISC7,	// 050
	MT_MISC8,	// 051
	MT_MISC9,	// 052
	MT_MISC10,	// 053
	MT_MISC11,	// 054
	MT_MISC12,	// 055
	MT_INV,	// 056
	MT_MISC13,	// 057
	MT_INS,	// 058
	MT_MISC14,	// 059
	MT_MISC15,	// 060
	MT_MISC16,	// 061
	MT_MEGA,	// 062
	MT_CLIP,	// 063
	MT_MISC17,	// 064
	MT_MISC18,	// 065
	MT_MISC19,	// 066
	MT_MISC20,	// 067
	MT_MISC21,	// 068
	MT_MISC22,	// 069
	MT_MISC23,	// 070
	MT_MISC24,	// 071
	MT_MISC25,	// 072
	MT_CHAINGUN,	// 073
	MT_MISC26,	// 074
	MT_MISC27,	// 075
	MT_MISC28,	// 076
	MT_SHOTGUN,	// 077
	MT_SUPERSHOTGUN,	// 078
	MT_MISC29,	// 079
	MT_MISC30,	// 080
	MT_MISC31,	// 081
	MT_MISC32,	// 082
	MT_MISC33,	// 083
	MT_MISC34,	// 084
	MT_MISC35,	// 085
	MT_MISC36,	// 086
	MT_MISC37,	// 087
	MT_MISC38,	// 088
	MT_MISC39,	// 089
	MT_MISC40,	// 090
	MT_MISC41,	// 091
	MT_MISC42,	// 092
	MT_MISC43,	// 093
	MT_MISC44,	// 094
	MT_MISC45,	// 095
	MT_MISC46,	// 096
	MT_MISC47,	// 097
	MT_MISC48,	// 098
	MT_MISC49,	// 099
	MT_MISC50,	// 100
	MT_MISC51,	// 101
	MT_MISC52,	// 102
	MT_MISC53,	// 103
	MT_MISC54,	// 104
	MT_MISC55,	// 105
	MT_MISC56,	// 106
	MT_MISC57,	// 107
	MT_MISC58,	// 108
	MT_MISC59,	// 109
	MT_MISC60,	// 110
	MT_MISC61,	// 111
	MT_MISC62,	// 112
	MT_MISC63,	// 113
	MT_MISC64,	// 114
	MT_MISC65,	// 115
	MT_MISC66,	// 116
	MT_MISC67,	// 117
	MT_MISC68,	// 118
	MT_MISC69,	// 119
	MT_MISC70,	// 120
	MT_MISC71,	// 121
	MT_MISC72,	// 122
	MT_MISC73,	// 123
	MT_MISC74,	// 124
	MT_MISC75,	// 125
	MT_MISC76,	// 126
	MT_MISC77,	// 127
	MT_MISC78,	// 128
	MT_MISC79,	// 129
	MT_MISC80,	// 130
	MT_MISC81,	// 131
	MT_MISC82,	// 132
	MT_MISC83,	// 133
	MT_MISC84,	// 134
	MT_MISC85,	// 135
	MT_MISC86,	// 136
	MT_LIGHTSOURCE,	// 137
	MT_TEMPSOUNDORIGIN,	// 138
	NUMMOBJTYPES	// 139
} mobjtype_t;

// Text.
typedef enum
{
	TXT_D_DEVSTR,	// 000
	TXT_D_CDROM,	// 001
	TXT_PRESSKEY,	// 002
	TXT_PRESSYN,	// 003
	TXT_QUITMSG,	// 004
	TXT_LOADNET,	// 005
	TXT_QLOADNET,	// 006
	TXT_QSAVESPOT,	// 007
	TXT_SAVEDEAD,	// 008
	TXT_QSPROMPT,	// 009
	TXT_QLPROMPT,	// 010
	TXT_NEWGAME,	// 011
	TXT_NIGHTMARE,	// 012
	TXT_SWSTRING,	// 013
	TXT_MSGOFF,	// 014
	TXT_MSGON,	// 015
	TXT_NETEND,	// 016
	TXT_ENDGAME,	// 017
	TXT_DOSY,	// 018
	TXT_DETAILHI,	// 019
	TXT_DETAILLO,	// 020
	TXT_GAMMALVL0,	// 021
	TXT_GAMMALVL1,	// 022
	TXT_GAMMALVL2,	// 023
	TXT_GAMMALVL3,	// 024
	TXT_GAMMALVL4,	// 025
	TXT_EMPTYSTRING,	// 026
	TXT_GOTARMOR,	// 027
	TXT_GOTMEGA,	// 028
	TXT_GOTHTHBONUS,	// 029
	TXT_GOTARMBONUS,	// 030
	TXT_GOTSTIM,	// 031
	TXT_GOTMEDINEED,	// 032
	TXT_GOTMEDIKIT,	// 033
	TXT_GOTSUPER,	// 034
	TXT_GOTBLUECARD,	// 035
	TXT_GOTYELWCARD,	// 036
	TXT_GOTREDCARD,	// 037
	TXT_GOTBLUESKUL,	// 038
	TXT_GOTYELWSKUL,	// 039
	TXT_GOTREDSKULL,	// 040
	TXT_GOTINVUL,	// 041
	TXT_GOTBERSERK,	// 042
	TXT_GOTINVIS,	// 043
	TXT_GOTSUIT,	// 044
	TXT_GOTMAP,	// 045
	TXT_GOTVISOR,	// 046
	TXT_GOTMSPHERE,	// 047
	TXT_GOTCLIP,	// 048
	TXT_GOTCLIPBOX,	// 049
	TXT_GOTROCKET,	// 050
	TXT_GOTROCKBOX,	// 051
	TXT_GOTCELL,	// 052
	TXT_GOTCELLBOX,	// 053
	TXT_GOTSHELLS,	// 054
	TXT_GOTSHELLBOX,	// 055
	TXT_GOTBACKPACK,	// 056
	TXT_GOTBFG9000,	// 057
	TXT_GOTCHAINGUN,	// 058
	TXT_GOTCHAINSAW,	// 059
	TXT_GOTLAUNCHER,	// 060
	TXT_GOTPLASMA,	// 061
	TXT_GOTSHOTGUN,	// 062
	TXT_GOTSHOTGUN2,	// 063
	TXT_PD_BLUEO,	// 064
	TXT_PD_REDO,	// 065
	TXT_PD_YELLOWO,	// 066
	TXT_PD_BLUEK,	// 067
	TXT_PD_REDK,	// 068
	TXT_PD_YELLOWK,	// 069
	TXT_GGSAVED,	// 070
	TXT_HUSTR_MSGU,	// 071
	TXT_HUSTR_E1M1,	// 072
	TXT_HUSTR_E1M2,	// 073
	TXT_HUSTR_E1M3,	// 074
	TXT_HUSTR_E1M4,	// 075
	TXT_HUSTR_E1M5,	// 076
	TXT_HUSTR_E1M6,	// 077
	TXT_HUSTR_E1M7,	// 078
	TXT_HUSTR_E1M8,	// 079
	TXT_HUSTR_E1M9,	// 080
	TXT_HUSTR_E2M1,	// 081
	TXT_HUSTR_E2M2,	// 082
	TXT_HUSTR_E2M3,	// 083
	TXT_HUSTR_E2M4,	// 084
	TXT_HUSTR_E2M5,	// 085
	TXT_HUSTR_E2M6,	// 086
	TXT_HUSTR_E2M7,	// 087
	TXT_HUSTR_E2M8,	// 088
	TXT_HUSTR_E2M9,	// 089
	TXT_HUSTR_E3M1,	// 090
	TXT_HUSTR_E3M2,	// 091
	TXT_HUSTR_E3M3,	// 092
	TXT_HUSTR_E3M4,	// 093
	TXT_HUSTR_E3M5,	// 094
	TXT_HUSTR_E3M6,	// 095
	TXT_HUSTR_E3M7,	// 096
	TXT_HUSTR_E3M8,	// 097
	TXT_HUSTR_E3M9,	// 098
	TXT_HUSTR_E4M1,	// 099
	TXT_HUSTR_E4M2,	// 100
	TXT_HUSTR_E4M3,	// 101
	TXT_HUSTR_E4M4,	// 102
	TXT_HUSTR_E4M5,	// 103
	TXT_HUSTR_E4M6,	// 104
	TXT_HUSTR_E4M7,	// 105
	TXT_HUSTR_E4M8,	// 106
	TXT_HUSTR_E4M9,	// 107
	TXT_HUSTR_1,	// 108
	TXT_HUSTR_2,	// 109
	TXT_HUSTR_3,	// 110
	TXT_HUSTR_4,	// 111
	TXT_HUSTR_5,	// 112
	TXT_HUSTR_6,	// 113
	TXT_HUSTR_7,	// 114
	TXT_HUSTR_8,	// 115
	TXT_HUSTR_9,	// 116
	TXT_HUSTR_10,	// 117
	TXT_HUSTR_11,	// 118
	TXT_HUSTR_12,	// 119
	TXT_HUSTR_13,	// 120
	TXT_HUSTR_14,	// 121
	TXT_HUSTR_15,	// 122
	TXT_HUSTR_16,	// 123
	TXT_HUSTR_17,	// 124
	TXT_HUSTR_18,	// 125
	TXT_HUSTR_19,	// 126
	TXT_HUSTR_20,	// 127
	TXT_HUSTR_21,	// 128
	TXT_HUSTR_22,	// 129
	TXT_HUSTR_23,	// 130
	TXT_HUSTR_24,	// 131
	TXT_HUSTR_25,	// 132
	TXT_HUSTR_26,	// 133
	TXT_HUSTR_27,	// 134
	TXT_HUSTR_28,	// 135
	TXT_HUSTR_29,	// 136
	TXT_HUSTR_30,	// 137
	TXT_HUSTR_31,	// 138
	TXT_HUSTR_32,	// 139
	TXT_PHUSTR_1,	// 140
	TXT_PHUSTR_2,	// 141
	TXT_PHUSTR_3,	// 142
	TXT_PHUSTR_4,	// 143
	TXT_PHUSTR_5,	// 144
	TXT_PHUSTR_6,	// 145
	TXT_PHUSTR_7,	// 146
	TXT_PHUSTR_8,	// 147
	TXT_PHUSTR_9,	// 148
	TXT_PHUSTR_10,	// 149
	TXT_PHUSTR_11,	// 150
	TXT_PHUSTR_12,	// 151
	TXT_PHUSTR_13,	// 152
	TXT_PHUSTR_14,	// 153
	TXT_PHUSTR_15,	// 154
	TXT_PHUSTR_16,	// 155
	TXT_PHUSTR_17,	// 156
	TXT_PHUSTR_18,	// 157
	TXT_PHUSTR_19,	// 158
	TXT_PHUSTR_20,	// 159
	TXT_PHUSTR_21,	// 160
	TXT_PHUSTR_22,	// 161
	TXT_PHUSTR_23,	// 162
	TXT_PHUSTR_24,	// 163
	TXT_PHUSTR_25,	// 164
	TXT_PHUSTR_26,	// 165
	TXT_PHUSTR_27,	// 166
	TXT_PHUSTR_28,	// 167
	TXT_PHUSTR_29,	// 168
	TXT_PHUSTR_30,	// 169
	TXT_PHUSTR_31,	// 170
	TXT_PHUSTR_32,	// 171
	TXT_THUSTR_1,	// 172
	TXT_THUSTR_2,	// 173
	TXT_THUSTR_3,	// 174
	TXT_THUSTR_4,	// 175
	TXT_THUSTR_5,	// 176
	TXT_THUSTR_6,	// 177
	TXT_THUSTR_7,	// 178
	TXT_THUSTR_8,	// 179
	TXT_THUSTR_9,	// 180
	TXT_THUSTR_10,	// 181
	TXT_THUSTR_11,	// 182
	TXT_THUSTR_12,	// 183
	TXT_THUSTR_13,	// 184
	TXT_THUSTR_14,	// 185
	TXT_THUSTR_15,	// 186
	TXT_THUSTR_16,	// 187
	TXT_THUSTR_17,	// 188
	TXT_THUSTR_18,	// 189
	TXT_THUSTR_19,	// 190
	TXT_THUSTR_20,	// 191
	TXT_THUSTR_21,	// 192
	TXT_THUSTR_22,	// 193
	TXT_THUSTR_23,	// 194
	TXT_THUSTR_24,	// 195
	TXT_THUSTR_25,	// 196
	TXT_THUSTR_26,	// 197
	TXT_THUSTR_27,	// 198
	TXT_THUSTR_28,	// 199
	TXT_THUSTR_29,	// 200
	TXT_THUSTR_30,	// 201
	TXT_THUSTR_31,	// 202
	TXT_THUSTR_32,	// 203
	TXT_HUSTR_CHATMACRO1,	// 204
	TXT_HUSTR_CHATMACRO2,	// 205
	TXT_HUSTR_CHATMACRO3,	// 206
	TXT_HUSTR_CHATMACRO4,	// 207
	TXT_HUSTR_CHATMACRO5,	// 208
	TXT_HUSTR_CHATMACRO6,	// 209
	TXT_HUSTR_CHATMACRO7,	// 210
	TXT_HUSTR_CHATMACRO8,	// 211
	TXT_HUSTR_CHATMACRO9,	// 212
	TXT_HUSTR_CHATMACRO0,	// 213
	TXT_HUSTR_TALKTOSELF1,	// 214
	TXT_HUSTR_TALKTOSELF2,	// 215
	TXT_HUSTR_TALKTOSELF3,	// 216
	TXT_HUSTR_TALKTOSELF4,	// 217
	TXT_HUSTR_TALKTOSELF5,	// 218
	TXT_HUSTR_MESSAGESENT,	// 219
	TXT_HUSTR_PLRGREEN,	// 220
	TXT_HUSTR_PLRINDIGO,	// 221
	TXT_HUSTR_PLRBROWN,	// 222
	TXT_HUSTR_PLRRED,	// 223
	TXT_AMSTR_FOLLOWON,	// 224
	TXT_AMSTR_FOLLOWOFF,	// 225
	TXT_AMSTR_GRIDON,	// 226
	TXT_AMSTR_GRIDOFF,	// 227
	TXT_AMSTR_MARKEDSPOT,	// 228
	TXT_AMSTR_MARKSCLEARED,	// 229
	TXT_STSTR_MUS,	// 230
	TXT_STSTR_NOMUS,	// 231
	TXT_STSTR_DQDON,	// 232
	TXT_STSTR_DQDOFF,	// 233
	TXT_STSTR_KFAADDED,	// 234
	TXT_STSTR_FAADDED,	// 235
	TXT_STSTR_NCON,	// 236
	TXT_STSTR_NCOFF,	// 237
	TXT_STSTR_BEHOLD,	// 238
	TXT_STSTR_BEHOLDX,	// 239
	TXT_STSTR_CHOPPERS,	// 240
	TXT_STSTR_CLEV,	// 241
	TXT_E1TEXT,	// 242
	TXT_E2TEXT,	// 243
	TXT_E3TEXT,	// 244
	TXT_E4TEXT,	// 245
	TXT_C1TEXT,	// 246
	TXT_C2TEXT,	// 247
	TXT_C3TEXT,	// 248
	TXT_C4TEXT,	// 249
	TXT_C5TEXT,	// 250
	TXT_C6TEXT,	// 251
	TXT_P1TEXT,	// 252
	TXT_P2TEXT,	// 253
	TXT_P3TEXT,	// 254
	TXT_P4TEXT,	// 255
	TXT_P5TEXT,	// 256
	TXT_P6TEXT,	// 257
	TXT_T1TEXT,	// 258
	TXT_T2TEXT,	// 259
	TXT_T3TEXT,	// 260
	TXT_T4TEXT,	// 261
	TXT_T5TEXT,	// 262
	TXT_T6TEXT,	// 263
	TXT_CC_ZOMBIE,	// 264
	TXT_CC_SHOTGUN,	// 265
	TXT_CC_HEAVY,	// 266
	TXT_CC_IMP,	// 267
	TXT_CC_DEMON,	// 268
	TXT_CC_LOST,	// 269
	TXT_CC_CACO,	// 270
	TXT_CC_HELL,	// 271
	TXT_CC_BARON,	// 272
	TXT_CC_ARACH,	// 273
	TXT_CC_PAIN,	// 274
	TXT_CC_REVEN,	// 275
	TXT_CC_MANCU,	// 276
	TXT_CC_ARCH,	// 277
	TXT_CC_SPIDER,	// 278
	TXT_CC_CYBER,	// 279
	TXT_CC_HERO,	// 280
	TXT_QUITMESSAGE1,	// 281
	TXT_QUITMESSAGE2,	// 282
	TXT_QUITMESSAGE3,	// 283
	TXT_QUITMESSAGE4,	// 284
	TXT_QUITMESSAGE5,	// 285
	TXT_QUITMESSAGE6,	// 286
	TXT_QUITMESSAGE7,	// 287
	TXT_QUITMESSAGE8,	// 288
	TXT_QUITMESSAGE9,	// 289
	TXT_QUITMESSAGE10,	// 290
	TXT_QUITMESSAGE11,	// 291
	TXT_QUITMESSAGE12,	// 292
	TXT_QUITMESSAGE13,	// 293
	TXT_QUITMESSAGE14,	// 294
	TXT_QUITMESSAGE15,	// 295
	TXT_QUITMESSAGE16,	// 296
	TXT_QUITMESSAGE17,	// 297
	TXT_QUITMESSAGE18,	// 298
	TXT_QUITMESSAGE19,	// 299
	TXT_QUITMESSAGE20,	// 300
	TXT_QUITMESSAGE21,	// 301
	TXT_QUITMESSAGE22,	// 302
	TXT_JOINNET,	// 303
	TXT_SAVENET,	// 304
	TXT_CLNETLOAD,	// 305
	TXT_LOADMISSING,	// 306
	TXT_RENDER_GLOWFLATS,	// 307
	TXT_RENDER_GLOWTEXTURES,	// 308
	TXT_FINALEFLAT_E1,	// 309
	TXT_FINALEFLAT_E2,	// 310
	TXT_FINALEFLAT_E3,	// 311
	TXT_FINALEFLAT_E4,	// 312
	TXT_FINALEFLAT_C2,	// 313
	TXT_FINALEFLAT_C1,	// 314
	TXT_FINALEFLAT_C3,	// 315
	TXT_FINALEFLAT_C4,	// 316
	TXT_FINALEFLAT_C5,	// 317
	TXT_FINALEFLAT_C6,	// 318
	TXT_ASK_EPISODE,	// 319
	TXT_EPISODE1,	// 320
	TXT_EPISODE2,	// 321
	TXT_EPISODE3,	// 322
	TXT_EPISODE4,	// 323
	TXT_KILLMSG_SUICIDE,	// 324
	TXT_KILLMSG_WEAPON0,	// 325
	TXT_KILLMSG_PISTOL,	// 326
	TXT_KILLMSG_SHOTGUN,	// 327
	TXT_KILLMSG_CHAINGUN,	// 328
	TXT_KILLMSG_MISSILE,	// 329
	TXT_KILLMSG_PLASMA,	// 330
	TXT_KILLMSG_BFG,	// 331
	TXT_KILLMSG_CHAINSAW,	// 332
	TXT_KILLMSG_SUPERSHOTGUN,	// 333
	NUMTEXT	// 334
} textenum_t;

#endif
