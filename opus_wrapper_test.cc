// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>

#include "opus_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

constexpr int valid_sample_rates[] = {8000, 12000, 16000, 24000, 48000};
constexpr int valid_num_channels[] = {1, 2};
constexpr int some_valid_percentage_losses[] = {0, 1, 2, 5, 10, 20, 50};

constexpr int invalid_sample_rates[] = {-1, 44100, 96000, 192000};
constexpr int invalid_num_channels[] = {-2, 0, 3, 4};

// Tests that opus::Encoder can be constructed with various correct
// sample rates, number of channels, and percentage losses
TEST(OpusWrapperTest, ValidEncoderConfigs) {
  for (auto sample_rate : valid_sample_rates) {
    for (auto num_channels : valid_num_channels) {
      for (auto loss : some_valid_percentage_losses) {
        EXPECT_TRUE(
            opus::Encoder(sample_rate, num_channels, OPUS_APPLICATION_AUDIO)
                .valid());
        EXPECT_TRUE(opus::Encoder(sample_rate, num_channels,
                                  OPUS_APPLICATION_AUDIO, loss)
                        .valid());
      }
    }
  }
}

// Tests that opus::Encoder fails to construct successfully when passed an
// invalid sample rate or number of channels
TEST(OpusWrapperTest, InvalidEncoderConfigs) {
  // invalid sample rates
  for (auto sample_rate : invalid_sample_rates) {
    for (auto num_channels : valid_num_channels) {
      for (auto loss : some_valid_percentage_losses) {
        EXPECT_FALSE(
            opus::Encoder(sample_rate, num_channels, OPUS_APPLICATION_AUDIO)
                .valid());
        EXPECT_FALSE(opus::Encoder(sample_rate, num_channels,
                                   OPUS_APPLICATION_AUDIO, loss)
                         .valid());
      }
    }
  }

  // invalid number of channels
  for (auto sample_rate : valid_sample_rates) {
    for (auto num_channels : invalid_num_channels) {
      for (auto loss : some_valid_percentage_losses) {
        EXPECT_FALSE(
            opus::Encoder(sample_rate, num_channels, OPUS_APPLICATION_AUDIO)
                .valid());
        EXPECT_FALSE(opus::Encoder(sample_rate, num_channels,
                                   OPUS_APPLICATION_AUDIO, loss)
                         .valid());
      }
    }
  }
}

// Tests that opus::Decoder can be constructed with various correct
// sample rates and number of channels
TEST(OpusWrapperTest, ValidDecoderConfigs) {
  for (auto sample_rate : valid_sample_rates) {
    for (auto num_channels : valid_num_channels) {
      EXPECT_TRUE(opus::Decoder(sample_rate, num_channels).valid());
    }
  }
}

// Tests that opus::Decoder fails to construct successfully when passed an
// invalid sample rate or number of channels
TEST(OpusWrapperTest, InvalidDecoderConfigs) {
  // invalid sample rates
  for (auto sample_rate : invalid_sample_rates) {
    for (auto num_channels : valid_num_channels) {
      EXPECT_FALSE(opus::Decoder(sample_rate, num_channels).valid());
    }
  }

  // invalid number of channels
  for (auto sample_rate : valid_sample_rates) {
    for (auto num_channels : invalid_num_channels) {
      EXPECT_FALSE(opus::Decoder(sample_rate, num_channels).valid());
    }
  }
}

constexpr auto kFrameSize = 960;
constexpr auto kNumChannels = 2;
constexpr auto kSampleRate = 48000;

std::vector<opus_int16> make_dummy_audio() {
  std::vector<opus_int16> dummy_audio(kFrameSize * kNumChannels);
  std::srand(0);
  std::generate(dummy_audio.begin(), dummy_audio.end(), std::rand);
  return dummy_audio;
}

// Tests that encoding data and then decoding results in the same size packet
TEST(OpusWrapperTest, EncodeAndDecode) {
  auto dummy_audio = make_dummy_audio();

  opus::Encoder encoder(kSampleRate, kNumChannels, OPUS_APPLICATION_AUDIO);
  EXPECT_TRUE(encoder.valid());
  opus::Decoder decoder(kSampleRate, kNumChannels);
  EXPECT_TRUE(decoder.valid());

  auto encoded = encoder.Encode(dummy_audio, kFrameSize);
  auto decoded = decoder.Decode(encoded, kFrameSize, false);
  auto decoded2 = decoder.Decode({encoded}, kFrameSize, false);
  EXPECT_EQ(dummy_audio.size(), decoded.size());
  EXPECT_EQ(dummy_audio.size(), decoded2.size());
}

// Tests that encoding an odd sized pcm doesn't attempt to encode two full
// frames
TEST(OpusWrapperTest, EncodeTooLong) {
  auto dummy_audio = make_dummy_audio();
  dummy_audio.push_back(0);

  opus::Encoder encoder(kSampleRate, kNumChannels, OPUS_APPLICATION_AUDIO);
  EXPECT_TRUE(encoder.valid());
  opus::Decoder decoder(kSampleRate, kNumChannels);
  EXPECT_TRUE(decoder.valid());

  auto encoded = encoder.Encode(dummy_audio, kFrameSize);
  EXPECT_EQ(encoded.size(), 1);
}

// Tests that encoding data at a fixed bitrate and then decoding results in
// the same size packet
TEST(OpusWrapperTest, EncodeAndDecodeFixedBitrate) {
  auto dummy_audio = make_dummy_audio();

  opus::Encoder encoder(kSampleRate, kNumChannels, OPUS_APPLICATION_VOIP);
  EXPECT_TRUE(encoder.valid());
  constexpr int kBitrate = 24000;
  EXPECT_TRUE(encoder.SetBitrate(kBitrate));
  EXPECT_TRUE(encoder.SetVariableBitrate(0));
  opus::Decoder decoder(kSampleRate, kNumChannels);
  EXPECT_TRUE(decoder.valid());

  auto encoded = encoder.Encode(dummy_audio, kFrameSize);
  ASSERT_LT(0, encoded.size());
  EXPECT_EQ(kFrameSize * (kBitrate / 8) / kSampleRate, encoded[0].size());
  auto decoded = decoder.Decode(encoded, kFrameSize, false);
  auto decoded2 = decoder.Decode({encoded}, kFrameSize, false);
  EXPECT_EQ(dummy_audio.size(), decoded.size());
  EXPECT_EQ(dummy_audio.size(), decoded2.size());
}

// Tests that recovering with FEC results in the same size packet
TEST(OpusWrapperTest, EncodeAndDecodeWithFec) {
  auto dummy_audio = make_dummy_audio();

  opus::Encoder encoder(kSampleRate, kNumChannels, OPUS_APPLICATION_AUDIO);
  EXPECT_TRUE(encoder.valid());
  opus::Decoder decoder(kSampleRate, kNumChannels);
  EXPECT_TRUE(decoder.valid());

  encoder.Encode(dummy_audio, kFrameSize);
  auto encoded = encoder.Encode(dummy_audio, kFrameSize);
  auto decoded = decoder.Decode(encoded, kFrameSize, true);
  auto decoded2 = decoder.Decode({encoded}, kFrameSize, true);
  EXPECT_EQ(dummy_audio.size(), decoded.size());
  EXPECT_EQ(dummy_audio.size(), decoded2.size());
}

// Tests that recovering a totally lost frame results in the same size packet
TEST(OpusWrapperTest, DecodeDummy) {
  auto dummy_audio = make_dummy_audio();

  opus::Encoder encoder(kSampleRate, kNumChannels, OPUS_APPLICATION_AUDIO);
  EXPECT_TRUE(encoder.valid());
  opus::Decoder decoder(kSampleRate, kNumChannels);
  EXPECT_TRUE(decoder.valid());

  encoder.Encode(dummy_audio, kFrameSize);
  auto decoded = decoder.DecodeDummy(kFrameSize);
  EXPECT_EQ(dummy_audio.size(), decoded.size());
}

}  // namespace
