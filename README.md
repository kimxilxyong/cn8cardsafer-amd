# cn8cardsaver AMD

[![Github All Releases](https://img.shields.io/github/downloads/kimxilxyong/cn8cardsafer-amd/total.svg)](https://github.com/kimxilxyong/cn8cardsafer-amd/releases)
![version](https://img.shields.io/badge/version-1.1.0-blue.svg?cacheSeconds=2592000)
[![GitHub Release Date](https://img.shields.io/github/release-date-pre/xmrig/xmrig-amd.svg)](https://github.com/xmrig/xmrig-amd/releases)
[![GitHub license](https://img.shields.io/github/license/kimxilxyong/cn8cardsafer-amd.svg)](https://github.com/kimxilxyong/cn8cardsafer-amd/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/kimxilxyong/cn8cardsafer-amd.svg)](https://github.com/kimxilxyong/cn8cardsafer-amd/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/kimxilxyong/cn8cardsafer-amd.svg)](https://github.com/kimxilxyong/cn8cardsafer-amd/network)

CN8CardSaver-amd is a high performance CryptoNight OpenCL AMD miner forked from [XMRig-amd](https://github.com/xmrig/xmrig-amd).

cn8cardsaver is a miner for CryptoNight coins with GPU temperature and fan control support. With it you can keep your expensive cards cool and save.
Keep it below 65 C to be on the safe side. If it gets to 80 C or above you could be damaging your card.
Use the switches ```--max-gpu-temp=65 and --gpu-temp-falloff=9``` for example to reduce the GPU load.

##### New feature:
Both linux and windows versions can now control the gpu fans automatically. If the temperature gets too high the fans are turned to 100% and back to lower speeds if the temperature gets back down. Use --gpu-fan-level=N to control the fan logic. 
On linux you have to use the amdgpu-pro driver.

GPU mining part based on [psychocrypt](https://github.com/psychocrypt) code used in xmr-stak-amd.

### Temperature control:
#### Command line options
```
      --max-gpu-temp=N      Maximum temperature a GPU may reach before its cooled down (default 75)
      --gpu-temp-falloff=N  Amount of temperature to cool off before mining ramps up again (default 10)
      --gpu-fan-level=N     -1 disabled||0 automatic (default)||1..100 Fan speed in percent\n\
```

#### Table of contents
* [Features](#features)
* [Download](#download)
* [Usage](#usage)
* [Donations](#donations)
* [Contacts](#contacts)
* [Build](https://github.com/xmrig/xmrig-amd/wiki/Build)

## Features
* High performance.
* Official Windows support.
* Support for backup (failover) mining server.
* Most CryptoNight coins supported
* Automatic GPU configuration.
* GPU temperature management (option --max-gpu-temp, --gpu-temp-falloff)
* GPU fan control (option --gpu-fan-level)
* Nicehash support.
* It's open source software.

## Download
* Binary releases: https://github.com/kimxilxyong/cn8cardsaver-amd/releases
* Git tree: https://github.com/kimxilxyong/cn8cardsaver-amd.git
  * Clone with `git clone https://github.com/kimxilxyong/cn8cardsaver-amd.git`

## Usage

Example:
```
cn8cardsaver-amd.exe --max-gpu-temp=74 --gpu-temp-falloff=7 -o pool.hashvault.pro:5555 -u 422KmQPiuCE7GdaAuvGxyYScin46HgBWMQo4qcRpcY88855aeJrNYWd3ZqE4BKwjhA2BJwQY7T2p6CUmvwvabs8vQqZAzLN.Monero1 -p Monero1-amd-gh
```

Use [config.xmrig.com](https://config.xmrig.com/amd) to generate, edit or share configurations.

### Command line options
```
  -a, --algo=ALGO              specify the algorithm to use
                                 cryptonight
                                 cryptonight-lite
                                 cryptonight-heavy
  -o, --url=URL                URL of mining server
  -O, --userpass=U:P           username:password pair for mining server
  -u, --user=USERNAME          username for mining server
  -p, --pass=PASSWORD          password for mining server
      --rig-id=ID              rig identifier for pool-side statistics (needs pool support)
  -k, --keepalive              send keepalived for prevent timeout (needs pool support)
      --nicehash               enable nicehash.com support
      --tls                    enable SSL/TLS support (needs pool support)
      --tls-fingerprint=F      pool TLS certificate fingerprint, if set enable strict certificate pinning
  -r, --retries=N              number of times to retry before switch to backup server (default: 5)
  -R, --retry-pause=N          time to pause between retries (default: 5)
      --opencl-devices=N       list of OpenCL devices to use.
      --opencl-launch=IxW      list of launch config, intensity and worksize
      --opencl-strided-index=N list of strided_index option values for each thread
      --opencl-mem-chunk=N     list of mem_chunk option values for each thread
      --opencl-comp-mode=N     list of comp_mode option values for each thread
      --opencl-affinity=N      list of affinity GPU threads to a CPU
      --opencl-platform=N      OpenCL platform index
      --opencl-loader=N        path to OpenCL-ICD-Loader (OpenCL.dll or libOpenCL.so)
      --print-platforms        print available OpenCL platforms and exit
      --max-gpu-temp=N         Maximum temperature a GPU may reach before its cooled down (default 75)
      --gpu-temp-falloff=N     Amount of temperature to cool off before mining starts again (default 10)	  
      --gpu-fan-level=N        -1 disabled||0 automatic (default)||1..100 Fan speed in percent\n\
      --no-cache               disable OpenCL cache
      --no-color               disable colored output
      --variant                algorithm PoW variant
      --donate-level=N         donate level, default 5% (5 minutes in 100 minutes)
      --user-agent             set custom user-agent string for pool
  -B, --background             run the miner in the background
  -c, --config=FILE            load a JSON-format configuration file
  -l, --log-file=FILE          log all output to a file
  -S, --syslog                 use system log for output messages
      --print-time=N           print hashrate report every N seconds
      --api-port=N             port for the miner API
      --api-access-token=T     access token for API
      --api-worker-id=ID       custom worker-id for API
      --api-id=ID              custom instance ID for API
      --api-ipv6               enable IPv6 support for API
      --api-no-restricted      enable full remote access (only if API token set)
      --dry-run                test configuration and exit
  -h, --help                   display this help and exit
  -V, --version                output version information and exit
```


## Supported algos / coins

```
    { "cryptonight",           "cn",           xmrig::CRYPTONIGHT,       xmrig::VARIANT_AUTO   },
    { "cryptonight/0",         "cn/0",         xmrig::CRYPTONIGHT,       xmrig::VARIANT_0      },
    { "cryptonight/1",         "cn/1",         xmrig::CRYPTONIGHT,       xmrig::VARIANT_1      },
    { "cryptonight/xtl",       "cn/xtl",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_XTL    },
    { "cryptonight/msr",       "cn/msr",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_MSR    },
    { "cryptonight/xao",       "cn/xao",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_XAO    },
    { "cryptonight/rto",       "cn/rto",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_RTO    },
    { "cryptonight/2",         "cn/2",         xmrig::CRYPTONIGHT,       xmrig::VARIANT_2      },
    { "cryptonight/half",      "cn/half",      xmrig::CRYPTONIGHT,       xmrig::VARIANT_HALF   },
    { "cryptonight/xtlv9",     "cn/xtlv9",     xmrig::CRYPTONIGHT,       xmrig::VARIANT_HALF   },
    { "cryptonight/wow",       "cn/wow",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_WOW    },
    { "cryptonight/r",         "cn/r",         xmrig::CRYPTONIGHT,       xmrig::VARIANT_4      },
    { "cryptonight/rwz",       "cn/rwz",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_RWZ    },
    { "cryptonight/zls",       "cn/zls",       xmrig::CRYPTONIGHT,       xmrig::VARIANT_ZLS    },
    { "cryptonight/double",    "cn/double",    xmrig::CRYPTONIGHT,       xmrig::VARIANT_DOUBLE },

#   ifndef XMRIG_NO_AEON
    { "cryptonight-lite",      "cn-lite",      xmrig::CRYPTONIGHT_LITE,  xmrig::VARIANT_AUTO },
    { "cryptonight-light",     "cn-light",     xmrig::CRYPTONIGHT_LITE,  xmrig::VARIANT_AUTO },
    { "cryptonight-lite/0",    "cn-lite/0",    xmrig::CRYPTONIGHT_LITE,  xmrig::VARIANT_0    },
    { "cryptonight-lite/1",    "cn-lite/1",    xmrig::CRYPTONIGHT_LITE,  xmrig::VARIANT_1    },
#   endif

#   ifndef XMRIG_NO_SUMO
    { "cryptonight-heavy",      "cn-heavy",      xmrig::CRYPTONIGHT_HEAVY, xmrig::VARIANT_AUTO },
    { "cryptonight-heavy/0",    "cn-heavy/0",    xmrig::CRYPTONIGHT_HEAVY, xmrig::VARIANT_0    },
    { "cryptonight-heavy/xhv",  "cn-heavy/xhv",  xmrig::CRYPTONIGHT_HEAVY, xmrig::VARIANT_XHV  },
    { "cryptonight-heavy/tube", "cn-heavy/tube", xmrig::CRYPTONIGHT_HEAVY, xmrig::VARIANT_TUBE },
#   endif

#   ifndef XMRIG_NO_CN_PICO
    { "cryptonight-pico/trtl",  "cn-pico/trtl",  xmrig::CRYPTONIGHT_PICO, xmrig::VARIANT_TRTL },
    { "cryptonight-pico",       "cn-pico",       xmrig::CRYPTONIGHT_PICO, xmrig::VARIANT_TRTL },
    { "cryptonight-turtle",     "cn-trtl",       xmrig::CRYPTONIGHT_PICO, xmrig::VARIANT_TRTL },
    { "cryptonight-ultralite",  "cn-ultralite",  xmrig::CRYPTONIGHT_PICO, xmrig::VARIANT_TRTL },
    { "cryptonight_turtle",     "cn_turtle",     xmrig::CRYPTONIGHT_PICO, xmrig::VARIANT_TRTL },
#   endif

#   ifndef XMRIG_NO_CN_GPU
    { "cryptonight/gpu",        "cn/gpu",  xmrig::CRYPTONIGHT, xmrig::VARIANT_GPU },
#   endif
```


## Donations
Default donation 5% (5 minutes in 100 minutes) can be reduced to 1% via option `donate-level`.

* XMR: `422KmQPiuCE7GdaAuvGxyYScin46HgBWMQo4qcRpcY88855aeJrNYWd3ZqE4BKwjhA2BJwQY7T2p6CUmvwvabs8vQqZAzLN`
* BTC: `19hNKKFu34CniRWPhGqAB76vi3U4x7DZyZ`

#### Donate to xmrig dev
* XMR: `48edfHu7V9Z84YzzMa6fUueoELZ9ZRXq9VetWzYGzKt52XU5xvqgzYnDK9URnRoJMk1j8nLwEVsaSWJ4fhdUyZijBGUicoD`
* BTC: `1P7ujsXeX7GxQwHNnJsRMgAdNkFZmNVqJT`


## Release checksums (invalid, TBD)
### SHA-256
```

```

## Contacts
* kimxilxyong@gmail.com
* [reddit](https://www.reddit.com/user/kimilyong/)

