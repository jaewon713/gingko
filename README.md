# gingko
======

p2p tool

# gingko组成部分
* gko 客户端工具（当前基于libtorrent-1.0.0-RC2）
* gko-tracker tracker服务器
* 依赖libtorrent，需要打patch（libtorrent-1.0.0-RC2.patch）

# 编译
1. 从libtorrent官网下载libtorrent-1.0.0-RC2
   
    `http://sourceforge.net/projects/libtorrent/files/libtorrent/libtorrent-rasterbar-1.0.0-RC2.tar.gz/download`

2. 给libtorrent-1.0.0-RC2打patch
    
    `cd $libtorrent && patch -p1 < libtorrent-1.0.0-RC2.patch`

3. 编译gko（需设置好相关依赖库）

    `cd $gko && sh build.sh`

4. 编译gko-tracker
    `cd $gko-tracker && sh build.sh`

# 运行
1. 在服务端机器部署若干台gko-tracker，启动命令
    
    `./bin/noah_control start`

2. 在所有下载机器上启动做种客户端bbts-agent，启动命令

    `./NOAH/control start`

# 试用下载
1. 找一台起了bbts-agent做种进程的机器，启动做种任务
    
    `gko3 serve -p /path/to/a -S -1`
命令行取得infohash

2. 下载

    `gko3 down -i infohash -p /path/to/save -d 10M`

gko3命令行工具使用说明
====================
##gko3命令格式
	gko3 operation [options]
可用的operations如下：
<table border="0">
    <tr>
        <th width="15%">operation</th><th width="35%">说明</th>
        <th width="15%">operation</th><th width="35%">说明</th>
    </tr>
    <tr>
        <td>mkseed</td><td>制作种子文件</td>
        <td>dump</td><td>显示种子文件内容</td>
    </tr>
    <tr>
        <td>add</td><td>添加后台任务</td>
        <td>serve</td><td>制作种子文件并做种</td>
    </tr>
    <tr>
        <td>down</td><td colspan=3>下载数据</td>
    <tr>
        <td>list</td><td>显示任务状态</td>
        <td>cancel</td><td>取消任务</td>
    </tr>
    <tr>
        <td>getopt</td><td>查看限速等</td>
        <td>setopt</td><td>设置限速等</td></tr>
    <tr>
        <td>pause</td><td>暂停任务</td>
        <td>resume</td><td>恢复任务</td></tr>
    <tr>
</table>
##<a name="mkseed">mkseed</a>
像其他BT软件一样，BBTS下载的时候也需要有一个种子文件（.torrent）。因此，要下载一份数据，首先要在种子机上生成一个torrent种子文件：

    gko3 mkseed -p path [r:s:C:W:l] [--besthash] [--include-files regex] [--exclude-files regex]
<table border="0">
    <tr>
        <td width="10%">-p</td>
        <td width="15%">--path</td>
        <td width="20%">根目录或单个文件</td>
        <td width="55">每个种子文件中只能包含一个根目录或单文件，多个目录需首先归并到一个根目录下</td>
    </tr>
    <tr>
        <td>-r</td>
        <td>--torrent</td>
        <td>种子文件</td>
        <td>指定生成的种子文件名，如不指定，则以-p所指定的路径的最后一级名字作为前缀并以.torrent命令。如指定的path为/path/to/a，则默认生成的种子文件为${PWD}/a.torrent</td>
    </tr>
    <tr>
        <td>-s</td>
        <td>--size</td>
        <td>分块大小</td>
        <td>torrent中对共享文件piece的大小划分，必须是16KB的整数倍数，一般无须用户指定。</td>
    </tr>
    <tr>
        <td>-C</td>
        <td>--cluster</td>
        <td>集群种子源地址</td>
        <td>指定的集群地址将保存在torrent中，下载时可从该<a href="hadoopseed.md">hdfs</a>地址源下载。</td>
    </tr>
    <tr>
        <td>-W</td>
        <td>--webseed</td>
        <td>http种子源地址</td>
        <td>指定的http地址保存在torrent中，下载时可从http为源的地址下载</td>
    </tr>
    <tr>
        <td>-l</td>
        <td>--link</td>
        <td>软链是否跟随</td>
        <td>指定该参数则只保存软链本身。不指定该参数即默认跟随软链所链接的实际目录或文件。</td>
    </tr>
    <tr>
        <td></td>
        <td>--besthash</td>
        <td>优化hash计算</td>
        <td>对于大文件来说，指定该参数能加快制作种子文件的速度。</td>
    </tr>
    <tr>
        <td></td>
        <td>--include-files</td>
        <td>指定包含的文件</td>
        <td>指定包含哪些文件，支持正则表达式匹配，可指定多个。指定时需要匹配根路径下的完整path（包括根）。</td>
    </tr>
    <tr>
        <td></td>
        <td>--exclude-files</td>
        <td>指定不包含那些文件</td>
        <td>指定过滤掉哪些文件，支持正则表达式匹配，可指定多个。指定时需要匹配根路径下的完整path</td>
    </tr>
</table>
##<a name="dump">dump</a>
mkseed生成的种子文件是B编码格式的，如需查看torrent文件内容是否符合预期，可使用dump命令，格式如下：

    gko3 dump -r torrent
<table border="0">
    <tr>
        <td width="10%">-r</td>
        <td width="15%">--torrent</td>
        <td width="20%">种子文件</td>
        <td width="55">指定要查看的种子文件路径。</td>
    </tr>
</table>
##<a name="add">add</a>
添加后台任务，后台任务目前可以是做种任务，也可以是下载任务，不过建议只添加做种任务，下载任务统一使用sdown，废掉后台下载。

    gko3 add (-r torrent | -i infohash) (-p path | -n fullpath) -S seedtime [u:c:] [--seed] [--tracker tracker]
<table border="0">
    <tr>
        <td width="10%">-r</td>
        <td width="15%">--torrent</td>
        <td width="20%">种子文件</td>
        <td width="55">根据种子文件进行下载</td>
    </tr>
    <tr>
        <td>-i</td>
        <td>--infohash</td>
        <td>infohash</td>
        <td>根据infohash进行做种/下载，infohash即可通过mkseed/dump/serve中输出来获取，可唯一标志一份种子文件内容。</td>
    </tr>
    <tr>
        <td>-p</td>
        <td>--path</td>
        <td>数据保存目录</td>
        <td>对于做种任务：做种的源数据必须保存在此路径下；对于后台下载的任务：下载的数据会保存在该目录内</td>
    </tr>
    <tr>
        <td>-n</td>
        <td>--fullpath</td>
        <td>数据根目录/文件名</td>
        <td>该参数可更改（重命名）种子文件中数据的根目录/文件，指定该参数会忽略-p参数。</td>
    </tr>
    <tr>
        <td>-S</td>
        <td>--seedtime</td>
        <td>做种时间</td>
        <td>即成为种子后提供上传服务的时间，超时任务退出。若不提供该值，则会默认计算一个时间。-1为一直做种，直到用户cancel该任务</td>
    </tr>
    <tr>
        <td>-u</td>
        <td>--uplimit</td>
        <td>上传限速</td>
        <td>该任务共享给他人的最大带宽，单位MB/s</td>
    </tr>
    <tr>
        <td>-c</td>
        <td>--connlimit</td>
        <td>最大连接数</td>
        <td>该任务与其他peer建立连接的最大数目</td>
    </tr>
    <tr>
        <td></td>
        <td>--seed</td>
        <td>指定为做种任务</td>
        <td>默认不指定为做种任务，指定该参数表示该数据已经存在，只进行后台做种即可。</td>
    </tr>
    <tr>
        <td></td>
        <td>--tracker</td>
        <td>指定特定tracker</td>
        <td>请求特定的tracker，格式host:port，可指定多个，多个tracker按顺序连接，第一个不可用才会重试第二个</td>
    </tr>
</table>
##<a name="serve">serve</a>
为了方便做种，提供了serve命令，该命令集成了`mkseed + add -r *.torrent --seed`，即生成种子文件和做种放在一起。其包含的参数同**<a href="#mkseed">mkseed</a>**和**<a href="#add">add</a>**中的参数释义基本一致。唯一的区别在于-p所指定的参数：

    gko3 serve -p path -S seedtime [u:c:r:s:l] [--besthash] [--include-files] [--exclude-files]
<table border="0">
    <tr>
        <td width="10%">-p</td>
        <td width="15%">--path</td>
        <td width="20%">数据根目录或单个文件</td>
        <td width="55">该参数与mkseed中指定的-p参数相同，与add中的-n参数相同，有别于add中-p为数据根目录的上级目录</td>
    </tr>
    <tr>
        <td>-r</td>
        <td>--torrent</td>
        <td>种子文件</td>
        <td>指定生成的种子文件名，如不指定，则以-p所指定的路径的最后一级名字作为前缀并以.torrent命令。如指定的path为/path/to/a，则默认生成的种子文件为${PWD}/a.torrent</td>
    </tr>
    <tr>
        <td>-S</td>
        <td>--seedtime</td>
        <td>做种时间</td>
        <td>做种时间，单位s，做种完成后任务退出。做种任务必须提供该值，否则会直接退出。-1为一直做种，直到用户cancel该任务</td>
    </tr>
    <tr>
        <td>-u</td>
        <td>--uplimit</td>
        <td>上传限速</td>
        <td>该任务共享给他人的最大带宽，单位MB/s</td>
    </tr>
    <tr>
        <td>-c</td>
        <td>--connlimit</td>
        <td>最大连接数</td>
        <td>该任务与其他peer建立连接的最大数目</td>
    </tr>
    <tr>
        <td>-s</td>
        <td>--size</td>
        <td>分块大小</td>
        <td>torrent中对共享文件piece的大小划分，必须是16KB的整数倍数，一般无须用户指定。</td>
    </tr>
    <tr>
        <td>-l</td>
        <td>--link</td>
        <td>软链是否跟随</td>
        <td>指定该参数则只保存软链本身。不指定该参数即默认跟随软链所链接的实际目录或文件。</td>
    </tr>
    <tr>
        <td></td>
        <td>--besthash</td>
        <td>优化hash计算</td>
        <td>对于大文件来说，指定该参数能加快制作种子文件的速度。</td>
    </tr>
    <tr>
        <td></td>
        <td>--include-files</td>
        <td>指定包含的文件</td>
        <td>指定包含哪些文件，支持正则表达式匹配，可指定多个。指定时需要匹配根路径下的完整path（包括根）。</td>
    </tr>
    <tr>
        <td></td>
        <td>--exclude-files</td>
        <td>指定不包含那些文件</td>
        <td>指定过滤掉哪些文件，支持正则表达式匹配，可指定多个。指定时需要匹配根路径下的完整path</td>
    </tr>
    <tr>
        <td></td>
        <td>--tracker</td>
        <td>指定特定tracker</td>
        <td>请求特定的tracker，格式host:port，可指定多个，多个tracker按顺序连接，第一个不可用才会重试第二个</td>
    </tr>
</table>

##<a name="down">down</a>
下载使用down命令，命令格式如下：

	gko3 down (-r torrent | -i infohash) (-p path | -n fullpath) [S:O:d:u:c:C:W:P:] [--include-files] [--exclude-files]
下载时需要至少提供一种方式来指定种子源：

* 种子文件
* infohash
* http-url指定的种子文件地址
<table border="0">
    <tr>
        <td width="10%">-r</td>
        <td width="15%">--torrent</td>
        <td width="20%">种子文件</td>
        <td width="55">根据种子文件进行下载</td>
    </tr>
    <tr>
        <td>-i</td>
        <td>--infohash</td>
        <td>infohash</td>
        <td>根据infohash进行下载，infohash即可通过mkseed/dump/serve中输出来获取，可唯一标志一份种子文件内容。</td>
    </tr>
    <tr>
        <td>-p</td>
        <td>--path</td>
        <td>数据保存目录</td>
        <td>下载的数据会保存在该目录内</td>
    </tr>
    <tr>
        <td>-n</td>
        <td>--fullpath</td>
        <td>数据根目录/文件名</td>
        <td>该参数可更改（重命名）种子文件中数据的根目录/文件，指定该参数会忽略-p参数。</td>
    </tr>
    <tr>
        <td>-S</td>
        <td>--seedtime</td>
        <td>做种时间</td>
        <td>即下载完成后，继续做种的时间。若不提供该值，则会默认计算一个时间。-1会被忽略。</td>
    </tr>
    <tr>
        <td>-O</td>
        <td>--timeout</td>
        <td>超时时间</td>
        <td>如果超过该值还未完成下载，则下载进程退出。单位s</td>
    </tr>
    <tr>
        <td>-d</td>
        <td>--downlimit</td>
        <td>下载限速</td>
        <td>下载限速阈值，单位MB/s</td>
    </tr>
    <tr>
        <td>-u</td>
        <td>--uplimit</td>
        <td>上传限速</td>
        <td>上传限速阈值，单位MB/s</td>
    </tr>
    <tr>
        <td>-c</td>
        <td>--connlimit</td>
        <td>最大连接数</td>
        <td>该任务与其他peer建立连接的最大数目</td>
    </tr>
    <tr>
        <td>-C</td>
        <td>--cluster</td>
        <td>集群种子源地址</td>
        <td>指定该参数，下载将以<a href="hadoopseed.md">hdfs</a>作为一个确定的数据源进行下载</td>
    </tr>
    <tr>
        <td>-W</td>
        <td>--webseed</td>
        <td>http种子源地址</td>
        <td>指定该参数，下载将以http作为一个数据源进行下载，目前只支持种子为单文件，不支持目录。</td>
    </tr>
    <tr>
        <td>-P</td>
        <td>--port</td>
        <td>bind端口范围</td>
        <td>下载进程绑定的端口，供p2p上传数据使用。逗号分隔，e.g. -P 6881,6889</td>
    </tr>
    <tr>
        <td></td>
        <td>--offline</td>
        <td>绑定离线端口</td>
        <td>指定该参数，默认绑定sys指定的离线端口进行下载</td>
    </tr>
    <tr>
        <td></td>
        <td>--besthash</td>
        <td>优化hash计算</td>
        <td>对于大文件来说，指定该参数能加快制作种子文件的速度。</td>
    </tr>
    <tr>
        <td></td>
        <td>--include-files</td>
        <td>指定下载的文件</td>
        <td>指定下载哪些文件，支持正则表达式匹配，可指定多个。指定时需要匹配根路径下的完整path（包括根）。</td>
    </tr>
    <tr>
        <td></td>
        <td>--exclude-files</td>
        <td>指定下载过滤哪些文件</td>
        <td>指定过滤掉哪些文件不下载，支持正则表达式匹配，可指定多个。指定时需要匹配根路径下的完整path</td>
    </tr>
    <tr>
        <td></td>
        <td>--hang-timeout</td>
        <td>检测下载hang住超时值</td>
        <td>超过此时间没有下载1B数据，则认为下载没有数据源hang住，自动退出。默认15分钟</td>
    </tr>
    <tr>
        <td></td>
        <td>--progress</td>
        <td>输出下载进度</td>
        <td>在标准输出打印出进度，每2s刷新一次。</td>
    </tr>
    <tr>
        <td></td>
        <td>--continue</td>
        <td>快速恢复断点续传</td>
        <td>指定该参数，下载被中断，重试时可跳过check文件数据阶段，直接从上次进度开始下载</td>
    </tr>
    <tr>
        <td></td>
        <td>--tmp-files</td>
        <td>先下载到临时文件，在mv到正式目录</td>
        <td>下载时不直接下载到目标目录，而是先下载到目标目录下的临时目录中，下载成功在mv到正式目录下</td>
    </tr>
    <tr>
        <td></td>
        <td>--debug</td>
        <td>调试模式</td>
        <td>追问题是可打开调试模式，输出更多debug日志</td>
    </tr>
    <tr>
        <td></td>
        <td>--unix-socket</td>
        <td>指定与该下载进程通信的unix socket path</td>
        <td>通过指定该参数，用户可通过该unix socket path<strong>动态调整</strong>该下载进程的<strong>限速</strong>等</td>
    </tr>
    <tr>
        <td></td>
        <td>--save-torrent</td>
        <td>保存下载到的种子文件</td>
        <td>通过infohash下载时，使用该参数可将下载到的种子文件保存在本地指定文件中</td>
    </tr>
    <tr>
        <td></td>
        <td>--ip-write</td>
        <td>ip白名单</td>
        <td>只有在该白名单中的ip才会建立连接。格式: --ip-write=startip[-endip][,...]</td>
    </tr>
    <tr>
        <td></td>
        <td>--ip-black</td>
        <td>ip黑名单</td>
        <td>出现在该黑名单中的ip不会与之建立连接。格式: --ip-black=startip[-endip][,...]</td>
    </tr>
    <tr>
        <td></td>
        <td>--mask</td>
        <td>掩码</td>
        <td>类似子网掩码的机制，若下载机ip为10.26.38.49，指定--mask=255.255.0.0，则与指定--ip-white=10.26.0.0-10.26.255.255相同</td>
    </tr>
    <tr>
        <td></td>
        <td>--numwant</td>
        <td>一次获取的peers数目</td>
        <td>从tracker中一次获取的peers数目，默认25个</td>
    </tr>
    <tr>
        <td></td>
        <td>--ainterval</td>
        <td>与tracker通信的最大周期</td>
        <td>announce请求的最大周期，单位s，默认100s</td>
    </tr>
    <tr>
        <td></td>
        <td>--sndbuf</td>
        <td>发送缓冲区大小</td>
        <td>与其他peers通信的发送缓冲区大小</td>
    </tr>
    <tr>
        <td></td>
        <td>--rcvbuf</td>
        <td>接收缓冲区大小</td>
        <td>与其他peers通信的接收缓冲区大小</td>
    </tr>
    <tr>
        <td></td>
        <td>--hdfs-thread</td>
        <td>hdfs下载线程数</td>
        <td>从hdfs上下载时，起的线程数目</td>
    </tr>
    <tr>
        <td></td>
        <td>--tracker</td>
        <td>指定特定tracker</td>
        <td>请求特定的tracker，格式host:port，可指定多个，多个tracker按顺序连接，第一个不可用才会重试第二个</td>
    </tr>
    <tr>
        <td></td>
        <td>--tracker-failed-quit</td>
        <td>连不上tracker退出</td>
        <td>若连不上所有tracker，则报错退出</td>
    </tr>
    <tr>
        <td></td>
        <td>--use-dio-read</td>
        <td>磁盘读使用DIO</td>
        <td>所有读磁盘操作为DIO方式</td>
    </tr>
    <tr>
        <td></td>
        <td>--use-dio-write</td>
        <td>磁盘写使用DIO</td>
        <td>所有写磁盘操作为DIO方式</td>
    </tr>
    <tr>
        <td></td>
        <td>--ignore-hdfs-error</td>
        <td>忽略hdfs错误</td>
        <td>从hdfs下载数据时，若出现读失败等，暂不退出，尝试从其他peer获取数据，并每15s检测一下hdfs上数据是否可用</td>
    </tr>
    <tr>
        <td></td>
        <td>--mem-limit</td>
        <td>内存阈值</td>
        <td>超过指定的阈值则下载进程退出，检测周期为15s，单位MB</td>
    </tr>
    <tr>
        <td></td>
        <td>--pre-allocate</td>
        <td>磁盘空间预分配</td>
        <td>下载前对文件的磁盘空间预分配，使用该模式可减少文件的碎片数，但相应的会延长下载时间</td>
    </tr>
    <tr>
        <td></td>
        <td>--d-allocate</td>
        <td>动态磁盘空间预分配</td>
        <td>该参数和--pre-allocate类似，不过是边下载边分配空间，相比--pre-allocate，可以减少下载时间，但同时也会增加文件的碎片数。该参数需要指定每次预分配的块数，实测30-50能在时间和碎片数上达到较好的平衡</td>
    </tr>
</table>

##<a name="list">list</a>
list命令可查看目前所有后台任务

    gko3 list [-t taskid ...] [-a attributes] [--noheader]
<table border="0">
    <tr>
        <td width="10%">-t</td>
        <td width="15%">--task</td>
        <td width="20%">后台任务id</td>
        <td width="55">不指定-t则显示所有任务信息，否则显示特定的任务信息，批处理可指定多个-t</td>
    </tr>
    <tr>
        <td>-a</td>
        <td>--attr</td>
        <td>显示属性</td>
        <td>需要显示的<a href="#attr">任务属性</a></td>
    </tr>
    <tr>
        <td></td>
        <td>--noheader</td>
        <td>不显示标题</td>
        <td>输出不显示标题行</td>
    </tr>
</table>
###<a name="attr">任务属性</a>
目前支持的属性标题如下表所示，通过-a参数来指定，多个参数以逗号分隔。或以分号、空白符分隔，不过命令行中参数整体需加引号。

<table border="0">
    <tr>
        <td width="20%">taskid</td>
        <td width="30%">任务id</td>
        <td width="20%">status</td>
        <td width="30">任务状态</td>
    </tr>
    <tr>
        <td>infohash</td>
        <td>种子文件的infohash</td>
        <td>progress</td>
        <td>下载进度百分比</td>
    </tr>
    <tr>
        <td>user</td>
        <td>添加任务的属主</td>
        <td>group</td>
        <td>添加任务的属组</td>
    </tr>
    <tr>
        <td>cmd</td>
        <td>添加任务的完整命令行</td>
        <td>error</td>
        <td>若有错误，返回错误信息</td>
    </tr>
    <tr>
        <td>upload</td>
        <td>上传数据总量，单位bytes</td>
        <td>uprate</td>
        <td>当前上传速度，单位B/s</td>
    </tr>
    <tr>
        <td>peers</td>
        <td>本任务已知的peers数目</td>
        <td>seeds</td>
        <td>本任务已知的种子任务数</td>
    </tr>
    <tr>
        <td>savepath</td>
        <td>数据保存根路径</td>
        <td></td>
        <td></td>
    </tr>
</table>
##<a name="cancel">cancel/pause/resume</a>
* **cancel** 取消后台某个/某些任务
* **pause** 暂停后台某个/某些任务
* **resume** 恢复后台某个/某些任务

这三个命令，除root用户外，其他用户只能操作本用户创建的任务。

    gko3 cancel [-t taskid ...] [--all]
    gko3 pause  [-t taskid ...] [--all]
    gko3 resume [-t taskid ...] [--all]
<table border="0">
    <tr>
        <td width="10%">-t</td>
        <td width="15%">--task</td>
        <td width="20%">后台任务id</td>
        <td width="55">-t和--all必须指定其一；否则显示特定的任务信息，批处理可指定多个-t</td>
    </tr>
    <tr>
        <td></td>
        <td>--all</td>
        <td>所有任务</td>
        <td>指定所有本用户所添加的任务</td>
    </tr>
</table>
##<a name="getopt">getopt</a>
获取bbts-agent全局限速设置，或某个后台做种任务/下载任务的限速信息等。

    gko3 getopt [-t taskid | --unix-socket path]
不指定-t或--unix-socket则输出全局限速信息；指定-t，输出特定做种任务的限速信息。指定--unix-socket则输出该下载任务的限速信息。该参数需要与down中该参数搭配使用。
##<a name="setopt">setopt</a>
修改bbts-agent全局限速，或某个后台做种任务的限速，或设定unix sockt path的一个下载任务的限速。目前只有root用户可以修改全局限速，其他用户只能修改本用户添加任务的配置信息。

    gko3 setopt [-t taskid | --unix-socket path] [-d downrate] [-u uprate] [-c connlimit]

