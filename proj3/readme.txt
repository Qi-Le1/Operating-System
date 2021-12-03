1. 本来想写个LRU，但是这个只有fault才会进来，LRU和fifo比就wirte会变下顺序。 
2. 新设计的：如果npage多，nframs少，则碰撞次数多，反之则少。用这个ratio当一个系数，如果ratio大， 我们调整间隔就小，ratio小，调整间隔就大。先从0开始，前n次fault使用fifo,并统计每个frame fault的次数。n次之后，我们选择最少的evict那个，并每次最少的会增加1。同时用新列表看调整后的效果。当2n次的时候，用新列表中最少的。一直延续下去。让我们称之为 Dynamic Adjustable Frame Allocation (名字一定要高大上!)。n先设为500。
3. 还未debug