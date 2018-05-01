/** @file

  Copyright (c) 2018, Linaro Ltd. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <IndustryStandard/Acpi.h>

#include <AmdStyxAcpiLib.h>

#define FIELD_OFFSET(type, name)            __builtin_offsetof(type, name)

#pragma pack(1)
typedef struct {
  EFI_ACPI_6_2_PPTT_STRUCTURE_PROCESSOR                     Core;
  UINT32                                                    Offset[2];
  EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE                         DCache;
  EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE                         ICache;
} STYX_PPTT_CORE;

typedef struct {
  EFI_ACPI_6_2_PPTT_STRUCTURE_PROCESSOR                     Cluster;
  UINT32                                                    Offset[1];
  EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE                         L2Cache;
  STYX_PPTT_CORE                                            Cores[2];
} STYX_PPTT_CLUSTER;

typedef struct {
  EFI_ACPI_6_2_PPTT_STRUCTURE_PROCESSOR                     Package;
  UINT32                                                    Offset[1];
  EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE                         L3Cache;
  STYX_PPTT_CLUSTER                                         Clusters[NUM_CORES / 2];
} STYX_PPTT_PACKAGE;

typedef struct {
  EFI_ACPI_6_2_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER   Pptt;
  STYX_PPTT_PACKAGE                                         Packages[1];
} STYX_PPTT_TABLE;
#pragma pack()

#define PPTT_CORE(pid, cid, id) {                                             \
  {                                                                           \
    EFI_ACPI_6_2_PPTT_TYPE_PROCESSOR,                                         \
    FIELD_OFFSET (STYX_PPTT_CORE, DCache),                                    \
    {},                                                                       \
    {                                                                         \
      0,                                        /* PhysicalPackage */         \
      EFI_ACPI_6_2_PPTT_PROCESSOR_ID_VALID,     /* AcpiProcessorIdValid */    \
    },                                                                        \
    FIELD_OFFSET (STYX_PPTT_TABLE,                                            \
                  Packages[pid].Clusters[cid]), /* Parent */                  \
    ((cid) << 8) + (id),                        /* AcpiProcessorId */         \
    2,                                          /* NumberOfPrivateResources */\
  }, {                                                                        \
    FIELD_OFFSET (STYX_PPTT_TABLE,                                            \
                  Packages[pid].Clusters[cid].Cores[id].DCache),              \
    FIELD_OFFSET (STYX_PPTT_TABLE,                                            \
                  Packages[pid].Clusters[cid].Cores[id].ICache),              \
  }, {                                                                        \
    EFI_ACPI_6_2_PPTT_TYPE_CACHE,                                             \
    sizeof (EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE),                               \
    {},                                                                       \
    {                                                                         \
      1,          /* SizePropertyValid */                                     \
      1,          /* NumberOfSetsValid */                                     \
      1,          /* AssociativityValid */                                    \
      0,          /* AllocationTypeValid */                                   \
      1,          /* CacheTypeValid */                                        \
      1,          /* WritePolicyValid */                                      \
      1,          /* LineSizeValid */                                         \
    },                                                                        \
    0,            /* NextLevelOfCache */                                      \
    SIZE_32KB,    /* Size */                                                  \
    256,          /* NumberOfSets */                                          \
    2,            /* Associativity */                                         \
    {                                                                         \
      0,                                                /* AllocationType */  \
      EFI_ACPI_6_2_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,                          \
      EFI_ACPI_6_2_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,                  \
    },                                                                        \
    64            /* LineSize */                                              \
  }, {                                                                        \
    EFI_ACPI_6_2_PPTT_TYPE_CACHE,                                             \
    sizeof (EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE),                               \
    {},                                                                       \
    {                                                                         \
      1,          /* SizePropertyValid */                                     \
      1,          /* NumberOfSetsValid */                                     \
      1,          /* AssociativityValid */                                    \
      0,          /* AllocationTypeValid */                                   \
      1,          /* CacheTypeValid */                                        \
      1,          /* WritePolicyValid */                                      \
      1,          /* LineSizeValid */                                         \
    },                                                                        \
    0,            /* NextLevelOfCache */                                      \
    3 * SIZE_16KB,/* Size */                                                  \
    256,          /* NumberOfSets */                                          \
    3,            /* Associativity */                                         \
    {                                                                         \
      0,                                                /* AllocationType */  \
      EFI_ACPI_6_2_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,                   \
      EFI_ACPI_6_2_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,                  \
    },                                                                        \
    64            /* LineSize */                                              \
  }                                                                           \
}

#define PPTT_CLUSTER(pid, cid) {                                              \
  {                                                                           \
    EFI_ACPI_6_2_PPTT_TYPE_PROCESSOR,                                         \
    FIELD_OFFSET (STYX_PPTT_CLUSTER, L2Cache),                                \
    {},                                                                       \
    {                                                                         \
      0,                                      /* PhysicalPackage */           \
      EFI_ACPI_6_2_PPTT_PROCESSOR_ID_INVALID, /* AcpiProcessorIdValid */      \
    },                                                                        \
    FIELD_OFFSET (STYX_PPTT_TABLE, Packages[pid]), /* Parent */               \
    0,                                        /* AcpiProcessorId */           \
    1,                                        /* NumberOfPrivateResources */  \
  }, {                                                                        \
    FIELD_OFFSET (STYX_PPTT_TABLE, Packages[pid].Clusters[cid].L2Cache),      \
  }, {                                                                        \
    EFI_ACPI_6_2_PPTT_TYPE_CACHE,                                             \
    sizeof (EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE),                               \
    {},                                                                       \
    {                                                                         \
      1,          /* SizePropertyValid */                                     \
      1,          /* NumberOfSetsValid */                                     \
      1,          /* AssociativityValid */                                    \
      0,          /* AllocationTypeValid */                                   \
      1,          /* CacheTypeValid */                                        \
      1,          /* WritePolicyValid */                                      \
      1,          /* LineSizeValid */                                         \
    },                                                                        \
    0,            /* NextLevelOfCache */                                      \
    SIZE_1MB,     /* Size */                                                  \
    1024,         /* NumberOfSets */                                          \
    16,           /* Associativity */                                         \
    {                                                                         \
      0,                                                /* AllocationType */  \
      EFI_ACPI_6_2_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,                       \
      EFI_ACPI_6_2_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,                  \
    },                                                                        \
    64            /* LineSize */                                              \
  }, {                                                                        \
    PPTT_CORE(pid, cid, 0),                                                   \
    PPTT_CORE(pid, cid, 1),                                                   \
  }                                                                           \
}

STATIC STYX_PPTT_TABLE mStyxPpttTable = {
  {
    AMD_ACPI_HEADER(EFI_ACPI_6_2_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE,
                    STYX_PPTT_TABLE,
                    EFI_ACPI_6_2_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_REVISION),
  },
  {
    {
      {
        EFI_ACPI_6_2_PPTT_TYPE_PROCESSOR,
        FIELD_OFFSET (STYX_PPTT_PACKAGE, L3Cache),
        {},
        {
          1,                                      /* PhysicalPackage */
          EFI_ACPI_6_2_PPTT_PROCESSOR_ID_INVALID, /* AcpiProcessorIdValid */
        },
        0,                                        /* Parent */
        0,                                        /* AcpiProcessorId */
        1,                                        /* NumberOfPrivateResources */
      }, {
        FIELD_OFFSET (STYX_PPTT_TABLE, Packages[0].L3Cache),
      }, {
        EFI_ACPI_6_2_PPTT_TYPE_CACHE,
        sizeof (EFI_ACPI_6_2_PPTT_STRUCTURE_CACHE),
        {},
        {
          1,                                      /* SizePropertyValid */
          1,                                      /* NumberOfSetsValid */
          1,                                      /* AssociativityValid */
          0,                                      /* AllocationTypeValid */
          1,                                      /* CacheTypeValid */
          1,                                      /* WritePolicyValid */
          1,                                      /* LineSizeValid */
        },
        0,                                        /* NextLevelOfCache */
        SIZE_8MB,                                 /* Size */
        8192,                                     /* NumberOfSets */
        16,                                       /* Associativity */
        {
          0,                                      /* AllocationType */
          EFI_ACPI_6_2_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,
          EFI_ACPI_6_2_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,
        },
        64                                        /* LineSize */
      }, {
        PPTT_CLUSTER (0, 0),
#if NUM_CORES > 3
        PPTT_CLUSTER (0, 1),
#if NUM_CORES > 5
        PPTT_CLUSTER (0, 2),
#if NUM_CORES > 7
        PPTT_CLUSTER (0, 3),
#endif
#endif
#endif
      }
    }
  }
};

EFI_ACPI_DESCRIPTION_HEADER *
PpttHeader (
  VOID
  )
{
  return (EFI_ACPI_DESCRIPTION_HEADER *)&mStyxPpttTable.Pptt.Header;
}
