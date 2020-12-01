// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __GUSS_BLUR_H__
#define __GUSS_BLUR_H__

const unsigned int blur_tab[3] = { 94742, 118318, 147761 };
const unsigned int blur_tab_5x5[11][6] = {
    {724, 6281, 12903, 54460, 111885, 229861,},  //1.2
    {2969, 13306, 21938, 59634, 98320, 162102,}, //1.1
    {23247, 33824, 38328, 49214, 55766, 63191,}, //0.5
    {36883, 39164, 39955, 41586, 42426, 43283,}, //0.2
    {38226, 39539, 39986, 40896, 41358, 41826,}, //0.15
    {39205, 39798, 39997, 40399, 40602, 40805,}, //0.1
    {39800, 39950, 40000, 40100, 40150, 40200,}, //0.05
    {39992, 39998, 40000, 40004, 40006, 40008,}, //0.01
    //{39996, 39999, 40000, 40002, 40003, 40004,},
    {40200, 40150, 40100, 40000,  39950, 39800,},
    {39800, 39950, 40000, 40100, 40150, 40200,},
    {229861, 111885, 54460, 12903, 6281, 724,},
};
#endif