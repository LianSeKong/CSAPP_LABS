# Dy

## LIFO (last-in-first-out) policy

* 在空闲块列表的开端插入空闲块
* 研究表明，碎片化比地址有序更糟糕
* 简单而且常量时间


## Address-ordered policy

* 插入空闲的块，使空闲的列表块始终按地址顺序排列: addr(prev) < addr(curr) < addr(next)
* 需要搜索
* 研究表明，碎片化比LIFO更低