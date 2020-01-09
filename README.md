# Lifetime-Adaptive-ECC
Realize  the Lifetime Adapative ECC in SSDsim supporting superblock 

The process of realize the corresponding function:

1. realized the corresponding codes: supporting superblock 
2. realized the function: store data in cross page
  代码相应的修改如下：
   (1) 上层的数据拼接——insert2_command_buffer()
   (2) flash 层的数据存储
   (3) 映射条目的管理
      
