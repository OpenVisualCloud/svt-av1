/*
* Copyright(c) 2019 Intel Corporation
* SPDX - License - Identifier: BSD - 2 - Clause - Patent
*/

// EbLambdaRateTables.h file contains all material to estimate the rate of intra, inter and skip modes
#ifndef EbLambdaRateTables_h
#define EbLambdaRateTables_h
#ifdef __cplusplus
extern "C" {
#endif

#define NUMBER_OF_INTRA_MODES_MINUS_TWO  34
#define LUMA_INTRA_MODE_BITS_GRE31       196608 // farctional number of bit required for signaling luma intra mode greater than 31
#define QP_NUM                           52

    // *Note - As of Oct 2011, the JCT-VC uses the PSNR forumula
    //  PSNR = (LUMA_WEIGHT * PSNRy + PSNRu + PSNRv) / (2+LUMA_WEIGHT)
#define LUMA_WEIGHT                      1
#define CHROMA_WEIGHT                    1
#define RATE_WEIGHT                      1

// Precision macros used in the mode decision
#define BIT_ESTIMATE_PRECISION           15
#define LAMBDA_PRECISION                 16
#define COST_PRECISION                   8
#define MD_SHIFT                         (BIT_ESTIMATE_PRECISION + LAMBDA_PRECISION - COST_PRECISION)
#define MD_OFFSET                        (1 << (MD_SHIFT-1))

// Precision macros for chroma weight
// *Note - As in JCTVC-G1102,
#define CHROMA_WEIGHT_PRECISION          16
#define CHROMA_WEIGHT_SHIFT              (CHROMA_WEIGHT_PRECISION - COST_PRECISION)
#define CHROMA_WEIGHT_OFFSET             (1 << (CHROMA_WEIGHT_SHIFT - 1))

// Precision macro used in the motion estimation search
#define ME_SHIFT                         (BIT_ESTIMATE_PRECISION + LAMBDA_PRECISION)

// Syntax element macro
#define  ZERO_COST                       0
#define  TU_SPLIT_ZERO                   0
#define  TU_SPLIT_TAB_OFFSET             4 // offset to parse transSubDivFlag tables
#define  CBF_TAB_OFFSET                  3 // offset to parse Cbf tables
#define  SPLIT_FLAG_ZERO                 0
#define  SPLIT_TAB_OFFSET                3 // offset to parse split flag tables
#define  SKIP_FLAG_ZERO                  0
#define  SKIP_FLAG_ONE                   1
#define  SKIP_TAB_OFFSET                 3 // offset to parse skip flag tables

// Lamda table for mode decision -
// GOP structure: IPPPP...
// lambda values are << 16
// From JCTVC-F802
    static const uint32_t lambda_mode_decision_ld_sad[QP_NUM] = {
        12456,   13982,   15694,   17616,
        19773,   22194,   24912,   27963,
        31388,   35231,   39546,   44389,
        49825,   55926,   62775,   70463,
        79092,   88777,   99649,  111852,
        125550,  140925,  158183,  177555,
        199298,  223705,  251100,  281851,
        316367,  355109,  398597,  447410,
        502201,  563701,  632733,  710219,
        797194,  894820, 1004401, 1127402,
        1265466, 1420438, 1594387, 1789639,
        2008802, 2254804, 2530932, 2840875,
        3188775, 3579278, 4017604, 4509608
    };
    static const uint32_t lambda_mode_decision_ld_sad_qp_scaling[QP_NUM] = {
        15756,    17685,     19851,      22282,
        25011,    28074,     31512,      35371,
        39702,    44564,     50022,      56148,
        63024,    70742,     79405,      89129,
        100044,   112295,    126047,     141483,
        158810,   178258,    200088,     224591,
        252095,   294521,    343068,     398597,
        462083,   534633,    617504,     712118,
        820090,   943252,   1083680,    1243728,
        1426063,  1600702,   1796727,    2016758,
        2263734,  2540956,   2852127,    3201404,
        3593454,  4033516,   4527469,    5081912,
        5704253,  6402808,   7186909,    8067033
    };
    static const uint32_t lambda_mode_decision_ld_sse[QP_NUM] = {
        2367,      2983,      3758,       4735,
        5966,      7516,      9470,      11931,
        15033,     18940,     23863,      30065,
        37880,     47726,     60130,      75760,
        95451,    120261,    151519,     190902,
        240522,    303038,    381805,     481044,
        606077,    763609,    962087,    1212154,
        1527218,   1924174,   2424308,    3054436,
        3848349,   4848615,   6108873,    7696697,
        9697231,  12217745,  15393394,   19394462,
        24435491, 30786789,  38788923,   48870981,
        61573578, 77577847,  97741962,  123147156,
        155155694,195483924, 246294311,  310311387
    };
    static const uint32_t lambda_mode_decision_ld_sse_qp_scaling[QP_NUM] = {
        3788,       4773,       6013,       7576,
        9545,      12026,      15152,      19090,
        24052,      30304,      38180,      48104,
        60608,      76361,      96209,     121215,
        152722,     192417,     242431,     305444,
        384835,     484862,     610887,     769670,
        969723,    1323589,    1795896,    2424308,
        3258065,    4361462,    5818339,    7737905,
        10262263,   13576123,   17919360,   23603205,
        31031139,   39096785,   49258862,   62062277,
        78193570,   98517724,  124124555,  156387139,
        197035449,  248249110,  312774279,  394070898,
        496498219,  625548558,  788141796,  992996439
    };

    // Chroma weight table for mode decision -
    // weight values are << 16
    // From JCTVC-G1102
    static const uint32_t chroma_weight_factor_ld[QP_NUM] = {
        65536,  65536,  65536,   65536, //QP 0-3
        65536,  65536,  65536,   65536, //QP 4-7
        65536,  65536,  65536,   65536, //QP 8-11
        65536,  65536,  65536,   65536, //QP 12-15
        65536,  65536,  65536,   65536, //QP 16-19
        65536,  65536,  65536,   65536, //QP 20-23
        65536,  65536,  65536,   65536, //QP 24-27
        65536,  65536,  82570,   82570, //QP 28-31
        82570,  82570, 104032,  104032, //QP 32-35
        104032, 131072, 131072,  165140, //QP 36-39
        165140, 208064, 208064,  262144, //QP 40-43
        330281, 330281, 416128,  524288, //QP 44-47
        524288, 660561, 832255, 1048576  //QP 48-51
    };
    static const uint32_t chroma_weight_factor_ld_qp_scaling[QP_NUM] = {
        65536, 65536, 65536, 65536, //QP 0-3
        65536, 65536, 65536, 65536, //QP 4-7
        65536, 65536, 65536, 65536, //QP 8-11
        65536, 65536, 65536, 65536, //QP 12-15
        65536, 65536, 65536, 65536, //QP 16-19
        65536, 65536, 65536, 65536, //QP 20-23
        65536, 65536, 65536, 65536, //QP 24-27
        65536, 65536, 87427, 87157, //QP 28-31
        86916, 86699,114435,113940, //QP 32-35
        113489,142988,136771,172320, //QP 36-39
        165140,208064,208064,262144, //QP 40-43
        330281,330281,416128,524288, //QP 44-47
        524288,660561,832255,1048576 //QP 48-51
    };

    // For I_SLICE in the base layer
    static const uint32_t lambda_mode_decision_i_slice_sad[QP_NUM] = {
        9973   ,    11194  ,    12565  ,    14104  ,
        15831  ,    17769  ,    19945  ,    22388  ,
        25130  ,    28207  ,    31661  ,    35539  ,
        39891  ,    44776  ,    50259  ,    56414  ,
        63323  ,    71078  ,    79782  ,    89552  ,
        100519 ,    112829 ,    126646 ,    142155 ,
        159564 ,    179104 ,    201038 ,    225657 ,
        253292 ,    284310 ,    319127 ,    358208 ,
        402075 ,    451314 ,    506583 ,    568620 ,
        638255 ,    716417 ,    804151 ,    902628 ,
        1013166,    1137241,    1276509,    1432833,
        1608301,    1805257,    2026332,    2274481,
        2553019,    2865667,    3216602,    3610514
    };
    static const uint32_t lambda_mode_decision_i_slice_sse[QP_NUM] = {
        1518,     1912,       2409,      3035,
        3824,     4818,       6070,      7648,
        9636,     12141,      15296,     19272,
        24281,    30592,      38544,     48562,
        61185,    77088,      97124,     122369,
        154175,   194249,     244738,    308351,
        388497,   489476,     616701,    776995,
        978952,   1233402,    1553990,   1957904,
        2466805,  3107979,    3915808,   4933610,
        6215959,  7831617,    9867219,   12431917,
        15663234, 19734438,   24863834,  31326468,
        39468876, 49727668,   62652936,  78937753,
        99455336, 125305872,  157875506, 198910673
    };

    // For Random Access
    static const uint32_t lambda_mode_decision_ra_sad[QP_NUM] = {
        10893,   12227,   13724,   15404,
        17291,   19408,   21785,   24453,
        27448,   30809,   34582,   38817,
        43570,   48906,   54895,   61618,
        69164,   77634,   87141,   97812,
        109790,  123236,  138327,  155267,
        174281,  195624,  219581,  246471,
        276654,  310534,  348563,  391249,
        439162,  492942,  553309,  621068,
        697126,  782497,  878323,  985885,
        1106618, 1242137, 1394251, 1564994,
        1756647, 1971769, 2213236, 2484273,
        2788503, 3129988, 3513293, 3943538
    };
    static const uint32_t lambda_mode_decision_ra_sse[QP_NUM] = {
        1810,      2281,      2874,      3621,
        4562,      5748,      7242,      9124,
        11496,     14483,     18248,     22991,
        28967,     36496,     45982,     57934,
        72992,     91964,     115868,    145984,
        183928,    231735,    291968,    367857,
        463471,    583936,    735714,    926941,
        1167873,   1471427,   1853882,   2335745,
        2942855,   3707765,   4671491,   5885710,
        7415529,   9342982,   11771419,  14831059,
        18685963,  23542839,  29662118,  37371927,
        47085677,  59324236,  74743853,  94171354,
        118648472, 149487707, 188342709, 237296943

    };

    static const uint32_t lambda_mode_decision_ra_sad_qp_scaling_l1[QP_NUM] = {
        13778,   15465,   17359,   19485,
        21871,   24550,   27556,   30931,
        34719,   38971,   43743,   49100,
        55113,   61862,   69438,   77941,
        87486,   98200,   110225,  123724,
        138875,  155882,  174972,  196399,
        220450,  257551,  300005,  348563,
        404080,  467523,  539991,  622729,
        717148,  824850,  947651,  1087609,
        1247056, 1399773, 1571192, 1763604,
        1979578, 2222002, 2494112, 2799547,
        3142385, 3527208, 3959157, 4444003,
        4988225, 5599093, 6284770, 7054415

    };
    static const uint32_t lambda_mode_decision_ra_sse_qp_scaling_l1[QP_NUM] = {
        2897,      3650,      4598,      5793,
        7299,      9196,      11587,     14598,
        18393,     23174,     29197,     36786,
        46347,     58394,     73571,     92694,
        116787,    147143,    185388,    233575,
        294285,    370776,    467149,    588571,
        741553,    1012156,   1373332,   1853882,
        2491462,   3335235,   4449318,   5917222,
        7847613,   10381741,  13703040,  18049510,
        23729694,  29897541,  37668542,  47459389,
        59795083,  75337083,  94918777,  119590165,
        150674167, 189837554, 239180331, 301348334,
        379675109, 478360662, 602696667, 759350218

    };
    static const uint32_t lambda_mode_decision_ra_sad_qp_scaling_l3[QP_NUM] = {
        19107,   21447,   24073,   27021,
        30330,   34045,   38214,   42893,
        48146,   54042,   60661,   68089,
        76427,   85787,   96293,   108085,
        121321,  136178,  152855,  171574,
        192585,  216169,  242642,  272356,
        305710,  357159,  416031,  483370,
        560358,  648338,  748833,  863569,
        994505,  1143861, 1314155, 1508242,
        1729356, 1941136, 2178852, 2445678,
        2745181, 3081362, 3458712, 3882273,
        4357704, 4891357, 5490363, 6162724,
        6917423, 7764545, 8715407, 9782714

    };
    static const uint32_t lambda_mode_decision_ra_sse_qp_scaling_l3[QP_NUM] = {
        5571,       7018,       8843,       11141,
        14037,      17685,      22282,      28074,
        35371,      44564,      56148,      70742,
        89129,      112295,     141483,     178258,
        224591,     282967,     356516,     449182,
        565934,     713032,     898364,     1131867,
        1426063,    1946455,    2641024,    3565158,
        4791273,    6413914,    8556380,    11379273,
        15091563,   19964887,   26352000,   34710595,
        45634028,   57495272,   72439503,   91268055,
        114990544,  144879007,  182536110,  229981087,
        289758013,  365072220,  459962175,  579516026,
        730144440,  919924350,  1159032053, 1460288881

    };
    static const uint32_t chroma_weight_factor_ra[QP_NUM] = {

        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  82570,  82570,
        82570,  82570,  104032, 104032,
        104032, 131072, 131072, 165140,
        165140, 208064, 208064, 262144,
        330281, 330281, 416128, 524288,
        524288, 660561, 832255, 1048576

    };
    static const uint32_t chroma_weight_factor_ra_qp_scaling_l1[QP_NUM] = {

        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  87427,  87157,
        86916,  86699,  114435, 113940,
        113489, 142988, 136771, 172320,
        165140, 208064, 208064, 262144,
        330281, 330281, 416128, 524288,
        524288, 660561, 832255, 1048576

    };
    static const uint32_t chroma_weight_factor_ra_qp_scaling_l3[QP_NUM] = {

        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  65536,  65536,
        65536,  65536,  87427,  87157,
        86916,  86699,  114435, 113940,
        113489, 142988, 136771, 172320,
        165140, 208064, 208064, 262144,
        330281, 330281, 416128, 524288,
        524288, 660561, 832255, 1048576

    };

    // Lambda Table for bit-depth 8
    static const uint32_t av1_lambda_mode_decision8_bit_sse[QINDEX_RANGE /*256*/] = {
        58, 234, 234, 297, 366, 443, 528, 528,
        619, 718, 825, 938, 1059, 1188, 1323, 1323,
        1466, 1617, 1774, 1939, 2112, 2291, 2478, 2478,
        2673, 2874, 3083, 3300, 3523, 3754, 3754, 3993,
        4238, 4491, 4752, 5019, 5294, 5294, 5577, 5866,
        6163, 6468, 6779, 6779, 7098, 7425, 7758, 8099,
        8448, 8448, 8803, 9166, 9537, 9914, 10299, 10299,
        10692, 11091, 11498, 11913, 11913, 12334, 12763, 13200,
        13643, 14094, 14094, 14553, 15018, 15491, 15972, 15972,
        16459, 16954, 17457, 17966, 17966, 18483, 19008, 19539,
        20078, 20078, 20625, 21178, 21739, 22308, 22308, 22883,
        23466, 24057, 24057, 24654, 25259, 25872, 26491, 26491,
        27753, 28394, 29700, 31034, 31713, 33091, 33792, 35214,
        35937, 37403, 38148, 39658, 40425, 41979, 42768, 44366,
        45177, 46819, 47652, 49338, 50193, 51054, 52800, 53683,
        55473, 57291, 59139, 61017, 62923, 65838, 67818, 69828,
        71866, 73934, 76032, 78158, 80314, 82500, 84714, 86958,
        89232, 91534, 95043, 98618, 101038, 104723, 108474, 111012,
        114873, 118800, 121454, 125491, 128219, 132366, 135168, 139425,
        145203, 149614, 154091, 158634, 163243, 167918, 172659, 177466,
        182339, 187278, 193966, 199059, 205953, 211200, 216513, 223699,
        229166, 234699, 242179, 249777, 257491, 265323, 271274, 279312,
        287466, 295738, 304128, 312634, 321258, 330000, 338858, 350097,
        359219, 368459, 380174, 389678, 399300, 411491, 423866, 433898,
        446603, 459492, 472563, 485818, 499257, 512878, 526683, 540672,
        554843, 572091, 586666, 604398, 619377, 637593, 656073, 674817,
        693825, 713097, 732633, 755758, 779243, 799659, 827291, 851854,
        876777, 905699, 935091, 964953, 999108, 1029966, 1065243, 1105137,
        1145763, 1187123, 1229217, 1276366, 1328814, 1382318, 1436878, 1501866,
        1568292, 1636154, 1715472, 1796666, 1884993, 1986218, 2090091, 2202291,
        2323258, 2459457, 2605713, 2768923, 2943658, 3137291, 3344091, 3579194,
        3829774, 4104334, 4420548, 4756843, 5140138, 5565354, 6026254, 6544618
    };
    // Lambda Table for bit-depth 10
    static const uint32_t av1lambda_mode_decision10_bit_sse[QINDEX_RANGE /*256*/] = {
        4, 19, 23, 39, 52, 66, 92, 111,
        143, 180, 220, 265, 314, 367, 424, 506,
        573, 644, 745, 825, 939, 1060, 1155, 1289,
        1394, 1541, 1695, 1856, 1982, 2156, 2338, 2527,
        2723, 2926, 3084, 3300, 3524, 3755, 3993, 4239,
        4492, 4686, 4952, 5225, 5506, 5794, 6089, 6315,
        6623, 6938, 7261, 7591, 7843, 8186, 8536, 8894,
        9167, 9537, 9915, 10300, 10593, 10991, 11396, 11705,
        12123, 12441, 12872, 13310, 13644, 14095, 14438, 14902,
        15373, 15731, 16215, 16583, 17080, 17457, 17967, 18354,
        18876, 19273, 19674, 20215, 20625, 21179, 21599, 22023,
        22595, 23029, 23614, 24057, 24505, 25108, 25565, 26026,
        26961, 28073, 29044, 30031, 31204, 32227, 33266, 34322,
        35575, 36667, 37775, 38900, 40041, 41199, 42373, 43564,
        44771, 45995, 47235, 48492, 49765, 51055, 52361, 53684,
        55023, 57063, 58907, 61017, 63164, 65104, 67321, 69323,
        71610, 73675, 76032, 78159, 80315, 82775, 84994, 87241,
        89518, 92115, 95044, 98318, 101648, 104724, 108160, 111651,
        114873, 118141, 121789, 125153, 128563, 132019, 135873, 140141,
        144839, 149245, 153716, 158254, 163244, 167919, 172660, 177467,
        181931, 188108, 193967, 199487, 205519, 211640, 217852, 223700,
        229625, 236093, 243123, 250256, 257978, 265324, 272273, 279818,
        287467, 296260, 304656, 313706, 322345, 331101, 339974, 350097,
        359794, 370205, 380175, 390875, 401117, 412721, 424490, 435793,
        447884, 459492, 472564, 485819, 499257, 512879, 526684, 541376,
        556985, 572092, 587400, 604399, 621640, 639123, 656073, 675604,
        694623, 714715, 735094, 756591, 779244, 802230, 827292, 852739,
        878571, 907523, 936018, 966835, 999108, 1032884, 1068210, 1106144,
        1145764, 1187124, 1232404, 1279614, 1331023, 1384571, 1441473, 1503040,
        1568292, 1639831, 1716726, 1799234, 1888939, 1986219, 2090092, 2205134,
        2329100, 2465467, 2610352, 2772111, 2946945, 3140684, 3349346, 3581006,
        3831649, 4112097, 4424575, 4763110, 5142310, 5567614, 6030956, 6551969
    };
    // Lambda Table for bit-depth 12
    static const uint32_t av1lambda_mode_decision12_bit_sse[QINDEX_RANGE /*256*/] = {
        1, 2, 5, 9, 16, 24, 36, 52,
        70, 92, 119, 152, 189, 231, 281, 335,
        395, 464, 539, 620, 706, 805, 902, 1013,
        1131, 1255, 1394, 1532, 1685, 1836, 2003, 2179,
        2349, 2539, 2735, 2939, 3150, 3355, 3581, 3814,
        4054, 4301, 4556, 4818, 5088, 5347, 5631, 5922,
        6220, 6507, 6819, 7139, 7446, 7780, 8100, 8448,
        8781, 9121, 9490, 9843, 10203, 10569, 10941, 11320,
        11705, 12097, 12495, 12899, 13310, 13700, 14123, 14553,
        14960, 15402, 15821, 16245, 16706, 17142, 17584, 18031,
        18484, 18975, 19439, 19909, 20351, 20832, 21318, 21810,
        22308, 22775, 23284, 23761, 24280, 24768, 25298, 25795,
        26804, 27793, 28840, 29865, 30908, 31969, 33048, 34145,
        35260, 36392, 37496, 38664, 39850, 41005, 42225, 43414,
        44619, 45841, 47131, 48386, 49658, 50893, 52197, 53517,
        54855, 56834, 58849, 60840, 62924, 64982, 67135, 69260,
        71418, 73610, 75768, 78025, 80247, 82500, 84854, 87171,
        89447, 91825, 94970, 98168, 101419, 104724, 108002, 111331,
        114711, 118141, 121538, 124983, 128477, 132019, 135520, 140051,
        144566, 149060, 153622, 158254, 162954, 167625, 172361, 177164,
        182033, 187797, 193651, 199594, 205519, 211530, 217517, 223586,
        229740, 235861, 243005, 250375, 257613, 264954, 272398, 279818,
        287338, 295999, 304656, 313304, 322073, 330963, 339835, 349814,
        359937, 370060, 380322, 390576, 400965, 412567, 424178, 435951,
        447724, 459654, 472728, 485986, 499426, 512879, 526510, 541552,
        556628, 571911, 587400, 604213, 621451, 638740, 656267, 675407,
        695022, 714715, 734888, 756799, 779244, 802016, 827074, 852518,
        878571, 907067, 936249, 966364, 999108, 1032884, 1067963, 1106144,
        1146020, 1187124, 1232138, 1279072, 1330747, 1384571, 1441186, 1503334,
        1568592, 1640444, 1716099, 1799876, 1888281, 1986219, 2090438, 2205490,
        2328369, 2464715, 2610738, 2772509, 2946534, 3140260, 3348470, 3581006,
        3831649, 4111612, 4424071, 4763633, 5142852, 5568743, 6031544, 6551356
    };

    static const uint32_t av1_lambda_mode_decision8_bit_sad[QINDEX_RANGE /*256*/] = {
        86, 173, 173, 194, 216, 238, 259, 259,
        281, 303, 324, 346, 368, 389, 411, 411,
        433, 454, 476, 498, 519, 541, 563, 563,
        584, 606, 628, 649, 671, 693, 693, 714,
        736, 758, 779, 801, 823, 823, 844, 866,
        888, 909, 931, 931, 953, 974, 996, 1018,
        1039, 1039, 1061, 1083, 1104, 1126, 1148, 1148,
        1169, 1191, 1213, 1234, 1234, 1256, 1278, 1299,
        1321, 1343, 1343, 1364, 1386, 1408, 1429, 1429,
        1451, 1473, 1494, 1516, 1516, 1538, 1559, 1581,
        1603, 1603, 1624, 1646, 1668, 1689, 1689, 1711,
        1733, 1754, 1754, 1776, 1798, 1819, 1841, 1841,
        1884, 1906, 1949, 1993, 2014, 2058, 2079, 2123,
        2144, 2188, 2209, 2253, 2274, 2318, 2339, 2383,
        2404, 2448, 2469, 2513, 2534, 2556, 2599, 2621,
        2664, 2707, 2751, 2794, 2837, 2902, 2946, 2989,
        3032, 3076, 3119, 3162, 3206, 3249, 3292, 3336,
        3379, 3422, 3487, 3552, 3596, 3661, 3726, 3769,
        3834, 3899, 3942, 4007, 4051, 4116, 4159, 4224,
        4311, 4376, 4441, 4506, 4571, 4636, 4701, 4766,
        4831, 4896, 4982, 5047, 5134, 5199, 5264, 5351,
        5416, 5481, 5567, 5654, 5740, 5827, 5892, 5979,
        6065, 6152, 6239, 6325, 6412, 6499, 6585, 6694,
        6780, 6867, 6975, 7062, 7149, 7257, 7365, 7452,
        7560, 7669, 7777, 7885, 7994, 8102, 8210, 8319,
        8427, 8557, 8665, 8795, 8903, 9033, 9163, 9293,
        9423, 9553, 9683, 9835, 9987, 10117, 10290, 10442,
        10593, 10767, 10940, 11113, 11308, 11481, 11676, 11893,
        12110, 12326, 12543, 12781, 13041, 13301, 13561, 13865,
        14168, 14471, 14818, 15164, 15533, 15944, 16356, 16789,
        17244, 17742, 18262, 18826, 19411, 20039, 20689, 21404,
        22140, 22920, 23787, 24675, 25650, 26690, 27773, 28943
    };
    // Lambda Table for bit-depth 10
    static const uint32_t av1lambda_mode_decision10_bit_sad[QINDEX_RANGE /*256*/] = {
        22, 49, 54, 70, 81, 91, 108, 119,
        135, 151, 167, 184, 200, 216, 232, 254,
        270, 287, 308, 324, 346, 368, 384, 406,
        422, 444, 465, 487, 503, 525, 547, 568,
        590, 611, 628, 649, 671, 693, 714, 736,
        758, 774, 796, 817, 839, 861, 882, 899,
        920, 942, 964, 985, 1001, 1023, 1045, 1066,
        1083, 1104, 1126, 1148, 1164, 1186, 1207, 1224,
        1245, 1261, 1283, 1305, 1321, 1343, 1359, 1381,
        1402, 1419, 1440, 1456, 1478, 1494, 1516, 1532,
        1554, 1570, 1586, 1608, 1624, 1646, 1662, 1678,
        1700, 1716, 1738, 1754, 1771, 1792, 1808, 1825,
        1857, 1895, 1928, 1960, 1998, 2031, 2063, 2096,
        2133, 2166, 2198, 2231, 2263, 2296, 2328, 2361,
        2393, 2426, 2458, 2491, 2523, 2556, 2588, 2621,
        2653, 2702, 2745, 2794, 2843, 2886, 2935, 2978,
        3027, 3070, 3119, 3162, 3206, 3255, 3298, 3341,
        3385, 3433, 3487, 3547, 3607, 3661, 3720, 3780,
        3834, 3888, 3948, 4002, 4056, 4110, 4170, 4235,
        4305, 4370, 4435, 4500, 4571, 4636, 4701, 4766,
        4825, 4906, 4982, 5053, 5128, 5204, 5280, 5351,
        5421, 5497, 5578, 5659, 5746, 5827, 5903, 5984,
        6065, 6158, 6244, 6336, 6423, 6510, 6596, 6694,
        6786, 6883, 6975, 7073, 7165, 7268, 7371, 7468,
        7571, 7669, 7777, 7885, 7994, 8102, 8210, 8324,
        8443, 8557, 8671, 8795, 8920, 9044, 9163, 9299,
        9429, 9564, 9700, 9840, 9987, 10133, 10290, 10447,
        10604, 10777, 10945, 11124, 11308, 11498, 11693, 11899,
        12110, 12326, 12559, 12798, 13052, 13312, 13583, 13870,
        14168, 14487, 14823, 15175, 15549, 15944, 16356, 16800,
        17266, 17764, 18279, 18836, 19421, 20050, 20705, 21409,
        22146, 22942, 23798, 24691, 25655, 26695, 27784, 28959
    };
    // Lambda Table for bit-depth 12
    static const uint32_t av1lambda_mode_decision12_bit_sad[QINDEX_RANGE /*256*/] = {
        11, 16, 25, 33, 45, 55, 67, 81,
        94, 108, 123, 139, 155, 171, 189, 207,
        224, 243, 262, 281, 300, 320, 339, 360,
        380, 400, 422, 442, 464, 484, 506, 528,
        548, 570, 591, 613, 634, 655, 677, 698,
        720, 741, 763, 785, 807, 827, 848, 870,
        892, 912, 934, 955, 976, 997, 1018, 1039,
        1060, 1080, 1102, 1122, 1142, 1163, 1183, 1203,
        1224, 1244, 1264, 1284, 1305, 1324, 1344, 1364,
        1383, 1404, 1423, 1441, 1462, 1481, 1500, 1519,
        1538, 1558, 1577, 1596, 1613, 1632, 1651, 1670,
        1689, 1707, 1726, 1743, 1762, 1780, 1799, 1817,
        1852, 1886, 1921, 1955, 1989, 2022, 2056, 2090,
        2124, 2158, 2190, 2224, 2258, 2290, 2324, 2357,
        2389, 2422, 2456, 2488, 2521, 2552, 2584, 2617,
        2649, 2697, 2744, 2790, 2838, 2884, 2931, 2977,
        3023, 3069, 3114, 3160, 3204, 3249, 3295, 3340,
        3383, 3428, 3486, 3544, 3603, 3661, 3718, 3774,
        3831, 3888, 3944, 3999, 4055, 4110, 4164, 4233,
        4301, 4368, 4434, 4500, 4567, 4632, 4697, 4762,
        4827, 4902, 4978, 5054, 5128, 5203, 5276, 5349,
        5422, 5494, 5577, 5661, 5742, 5823, 5904, 5984,
        6064, 6155, 6244, 6332, 6420, 6508, 6595, 6691,
        6787, 6882, 6977, 7070, 7164, 7266, 7368, 7470,
        7570, 7670, 7778, 7887, 7995, 8102, 8209, 8325,
        8440, 8555, 8671, 8794, 8918, 9042, 9165, 9297,
        9432, 9564, 9698, 9842, 9987, 10132, 10289, 10446,
        10604, 10775, 10947, 11121, 11308, 11498, 11691, 11899,
        12111, 12326, 12558, 12795, 13051, 13312, 13582, 13871,
        14169, 14490, 14820, 15178, 15546, 15944, 16357, 16801,
        17263, 17761, 18280, 18838, 19420, 20048, 20702, 21409,
        22146, 22940, 23796, 24693, 25657, 26698, 27785, 28958
    };


#ifdef __cplusplus
}
#endif
#endif //EbLambdaRateTables_h
