#include "widget.h"
#include "xbot.h"
#include "xnet.h"
#include "xtool.h"
#include "expend.h"
#include <locale>
#include <QStyleFactory>
#ifdef BODY_USE_CUBLAST
#include "utils/gpuchecker.h"
#endif

//回调函数,bot获取llama的日志传给ui,ui处理后最大ngl值再传回bot
static void bot_log_callback(ggml_log_level level, const char *text, void *user_data)
{
    xBot* bot = static_cast<xBot*>(user_data);//类型转换操作,不消耗资源,重新解释了现有内存地址的类型
    emit bot->bot2ui_log(QString::fromStdString(text));
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);//自适应缩放
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);//适配非整数倍缩放
    QApplication a(argc, argv);//事件实例
    QApplication::setStyle(QStyleFactory::create("Fusion"));//现代风格

    //------------------实例化主要节点------------------
    Widget w;//窗口实例
    Expend expend;//扩展实例
    xBot bot;//模型实例
    xNet net;//链接实例
    xTool tool;//工具实例

    //-----------------初始值设定-----------------------
    expend.wordsObj = bot.wordsObj = net.wordsObj = tool.wordsObj = w.wordsObj;//传递语言
    expend.max_thread = w.max_thread;
    w.whisper_model_path = QString::fromStdString(expend.whisper_params.model);
    w.voice_params = expend.voice_params;
    llama_log_set(bot_log_callback,&bot);//设置回调
#ifdef BODY_USE_CUBLAST
    gpuChecker gpuer;//监测显卡信息
#endif

    //------------------注册信号传递变量-------------------
    qRegisterMetaType<PARAMS>("PARAMS");//注册PARAMS作为信号传递变量
    qRegisterMetaType<QColor>("QColor");//注册QColor作为信号传递变量
    qRegisterMetaType<STATE>("STATE");//注册STATE作为信号传递变量
    qRegisterMetaType<QVector<Embedding_vector>>("QVector<Embedding_vector>");
    qRegisterMetaType<Voice_Params>("Voice_Params");

    //------------------连接模型和窗口-------------------
    QObject::connect(&bot,&xBot::bot2ui_params,&w,&Widget::recv_params);//bot将模型参数传递给ui
    QObject::connect(&bot,&xBot::bot2ui_output,&w,&Widget::reflash_output);//窗口输出区更新
    QObject::connect(&bot,&xBot::bot2ui_state,&w,&Widget::reflash_state);//窗口状态区更新
    QObject::connect(&bot,&xBot::bot2ui_play,&w,&Widget::recv_play);//播放加载动画
    QObject::connect(&bot,&xBot::bot2ui_loadover,&w,&Widget::recv_loadover);//完成加载模型
    QObject::connect(&bot,&xBot::bot2ui_pushover,&w,&Widget::recv_pushover);//完成推理
    QObject::connect(&bot,&xBot::bot2ui_resetover,&w,&Widget::recv_resetover);//完成重置,预解码约定
    QObject::connect(&bot,&xBot::bot2ui_stopover,&w,&Widget::recv_stopover);//完成停止
    QObject::connect(&bot,&xBot::bot2ui_arrivemaxctx,&w,&Widget::recv_arrivemaxctx);//模型达到最大上下文
    QObject::connect(&bot,&xBot::bot2ui_reload,&w,&Widget::recv_reload);//设置参数改变,重载模型
    QObject::connect(&bot,&xBot::bot2ui_datereset,&w,&Widget::recv_datereset);//bot发信号请求ui触发reset
    QObject::connect(&bot,&xBot::bot2ui_setreset,&w,&Widget::recv_setreset);//bot发信号请求ui触发reset
    QObject::connect(&bot,&xBot::bot2ui_tokens,&w,&Widget::recv_tokens);//传递测试解码token数量
    QObject::connect(&bot,&xBot::bot2ui_log,&w,&Widget::recv_log);//传递llama.cpp的log
    QObject::connect(&bot,&xBot::bot2ui_predecode,&w,&Widget::recv_predecode);//传递模型预解码的内容

    QObject::connect(&w, &Widget::ui2bot_loadmodel,&bot, [&bot]() {bot.start();});//开始加载模型,利用对象指针实现多线程
    QObject::connect(&w, &Widget::ui2bot_input,&bot,&xBot::recv_input);//传递用户输入
    QObject::connect(&w, &Widget::ui2bot_push,&bot, [&bot]() {bot.start();});//开始推理,利用对象指针实现多线程
    QObject::connect(&w, &Widget::ui2bot_reset,&bot,&xBot::recv_reset);//传递重置信号
    QObject::connect(&w, &Widget::ui2bot_stop,&bot,&xBot::recv_stop);//传递停止信号
    QObject::connect(&w, &Widget::ui2bot_date,&bot,&xBot::recv_date);//传递约定内容
    QObject::connect(&w, &Widget::ui2bot_set,&bot,&xBot::recv_set);//传递设置内容
    QObject::connect(&w, &Widget::ui2bot_language,&bot,&xBot::recv_language);//传递使用的语言
    QObject::connect(&w, &Widget::ui2bot_free,&bot,&xBot::recv_free);//释放模型和上下文
    QObject::connect(&bot,&xBot::bot2ui_vocab,&w,&Widget::recv_vocab);//传递模型词表
    QObject::connect(&bot,&xBot::bot2ui_kv,&w,&Widget::recv_kv);//传递缓存量
    QObject::connect(&w, &Widget::ui2bot_imagepath,&bot,&xBot::recv_imagepath);//传递图片路径
    QObject::connect(&w, &Widget::ui2bot_maxngl,&bot,&xBot::recv_maxngl);//传递模型最大的ngl值
#ifdef BODY_USE_CUBLAST
    QObject::connect(&gpuer,&gpuChecker::gpu_status,&w,&Widget::recv_gpu_status);//传递gpu信息
    QObject::connect(&gpuer,&gpuChecker::gpu_status,&bot,&xBot::recv_gpu_status);//传递gpu信息
    QObject::connect(&w, &Widget::gpu_reflash,&gpuer,&gpuChecker::encode_handleTimeout);//强制刷新gpu信息
#endif
    QObject::connect(&w, &Widget::ui2bot_dateset,&bot,&xBot::recv_dateset);//自动装载

    //------------------连接扩展和窗口-------------------
    QObject::connect(&w, &Widget::ui2expend_show,&expend,&Expend::recv_expend_show);//通知显示扩展窗口
    QObject::connect(&w, &Widget::ui2expend_log, &expend, &Expend::recv_log);
    QObject::connect(&w, &Widget::ui2expend_vocab, &expend, &Expend::recv_vocab);
    QObject::connect(&w, &Widget::ui2expend_voicedecode, &expend, &Expend::recv_voicedecode);//开始语音转文字
    QObject::connect(&expend, &Expend::expend2ui_voicedecode_over, &w, &Widget::recv_voicedecode_over);//转换完成返回结果
    QObject::connect(&expend, &Expend::expend2ui_whisper_modelpath, &w, &Widget::recv_whisper_modelpath);//传递模型路径
    QObject::connect(&expend, &Expend::expend2ui_state,&w,&Widget::reflash_state);//窗口状态区更新
    QObject::connect(&expend, &Expend::expend2ui_embeddingdb_describe, &w, &Widget::recv_embeddingdb_describe);//传递知识库的描述
    QObject::connect(&expend, &Expend::expend2ui_voiceparams,&w,&Widget::recv_voiceparams);//传递文转声参数

    //------------------连接net和窗口-------------------
    QObject::connect(&net,&xNet::net2ui_output,&w,&Widget::reflash_output);//窗口输出区更新
    QObject::connect(&net,&xNet::net2ui_state,&w,&Widget::reflash_state);//窗口状态区更新
    QObject::connect(&w, &Widget::ui2net_push,&net, [&net]() {net.start();});//开始推理,利用对象指针实现多线程
    QObject::connect(&net,&xNet::net2ui_pushover,&w,&Widget::recv_pushover);//完成推理
    QObject::connect(&w, &Widget::ui2net_apis,&net,&xNet::recv_apis);//传递api设置参数
    QObject::connect(&w, &Widget::ui2net_data,&net,&xNet::recv_data);//传递端点参数
    QObject::connect(&w, &Widget::ui2net_stop,&net,&xNet::recv_stop);//传递停止信号

    //------------------连接tool和窗口-------------------
    QObject::connect(&tool,&xTool::tool2ui_state,&w,&Widget::reflash_state);//窗口状态区更新
    QObject::connect(&tool,&xTool::tool2ui_pushover,&w,&Widget::recv_toolpushover);//完成推理
    QObject::connect(&w, &Widget::ui2tool_func_arg,&tool,&xTool::recv_func_arg);//传递函数名和参数
    QObject::connect(&w, &Widget::ui2tool_push,&tool, [&tool]() {tool.start();});//开始推理,利用对象指针实现多线程

    //------------------连接扩展和tool-------------------
    QObject::connect(&expend, &Expend::expend2tool_embeddingdb,&tool,&xTool::recv_embeddingdb);//传递已嵌入文本段数据
    QObject::connect(&expend, &Expend::expend2tool_serverapi,&tool,&xTool::recv_serverapi);//传递嵌入服务端点

    QObject::connect(&tool,&xTool::tool2expend_draw,&expend,&Expend::recv_draw);//开始绘制图像
    QObject::connect(&expend,&Expend::expend2tool_drawover,&tool,&xTool::recv_drawover);//图像绘制完成

    w.show();//展示窗口

    //---------------读取配置文件并执行------------------
    QFile configfile("./EVA_TEMP/eva_config.ini");
    if(configfile.exists())
    {
        QSettings settings("./EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        QString modelpath = settings.value("modelpath", "").toString();//模型路径

        QFile modelpath_file(modelpath);
        if(modelpath_file.exists())//模型存在的话才继续进行
        {
            // 读取配置文件中的值
            w.ui_SETTINGS.modelpath = modelpath;
            
            // ui显示值
            w.chattemplate_comboBox->setCurrentText(settings.value("chattemplate", "").toString());
            w.system_TextEdit->setText(settings.value("system_prompt", "").toString());
            w.input_pfx_LineEdit->setText(settings.value("input_pfx", "").toString());
            w.input_sfx_LineEdit->setText(settings.value("input_sfx", "").toString());
            w.cmd_checkbox->setChecked(settings.value("cmd_checkbox", "").toBool());
            w.calculator_checkbox->setChecked(settings.value("calculator_checkbox", "").toBool());
            w.knowledge_checkbox->setChecked(settings.value("knowledge_checkbox", "").toBool());
            w.positron_checkbox->setChecked(settings.value("positron_checkbox", "").toBool());
            w.stablediffusion_checkbox->setChecked(settings.value("stablediffusion_checkbox", "").toBool());
            w.toolguy_checkbox->setChecked(settings.value("toolguy_checkbox", "").toBool());
            if(settings.value("extra_lan", "").toString()!="zh"){w.switch_lan_change();}//切换为英文
            w.extra_TextEdit->setText(settings.value("extra_prompt", "").toString());//放到后面保护用户定义值

            w.temp_slider->setValue(settings.value("temp", "").toFloat()*100);
            w.repeat_slider->setValue(settings.value("repeat", "").toFloat()*100);
            w.npredict_slider->setValue(settings.value("npredict", "").toFloat());
            w.nthread_slider->setValue(settings.value("nthread", "").toInt());
            w.nctx_slider->setValue(settings.value("nctx", "").toInt());
            w.batch_slider->setValue(settings.value("batch", "").toInt());
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
            w.ngl_slider->setValue(settings.value("ngl", "").toInt());
#endif
            QFile checkFile(settings.value("lorapath", "").toString());
            if (checkFile.exists()) {w.lora_LineEdit->setText(settings.value("lorapath", "").toString());}
            QFile checkFile2(settings.value("mmprojpath", "").toString());
            if (checkFile2.exists()) {w.mmproj_LineEdit->setText(settings.value("mmprojpath", "").toString());}
            int mode_num = settings.value("ui_mode", "").toInt();
            if(mode_num == 0){w.chat_btn->setChecked(1);}
            else if(mode_num == 1){w.complete_btn->setChecked(1);}
            else if(mode_num == 2){w.web_btn->setChecked(1);}
            w.port_lineEdit->setText(settings.value("port", "").toString());

            // ui显示值传给ui内部值
            w.get_date();//获取约定中的纸面值
            w.get_set();//获取设置中的纸面值
            w.is_config = true;

            if(w.ui_mode == SERVER_){w.serverControl();}//自动启动服务
            else{emit w.ui2bot_dateset(w.ui_DATES,w.ui_SETTINGS);}//自动装载模型
            
        }
        
    }

    return a.exec();//进入事件循环
}