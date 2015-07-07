#!/bin/bash

rm -r test/data/scaling_test_*
mv build/test/scaling_test/Scaling_test_FTR-1_F-133* test/data/scaling_test_133_185
mv build/test/scaling_test/Scaling_test_FTR-1_F_* test/data/scaling_test_185_185
mv build/test/scaling_test/Scaling_test_FTR-1_F-S* test/data/scaling_test_239_185
mv build/test/scaling_test/Scaling_test_FTR-1_S-133* test/data/scaling_test_133_239
mv build/test/scaling_test/Scaling_test_FTR-1_S_* test/data/scaling_test_239_239
mv build/test/scaling_test/Scaling_test_FTR-1_S-F* test/data/scaling_test_185_239
