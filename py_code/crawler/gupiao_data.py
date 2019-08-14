#股票数据获取
#目标：获取上交所和深交所所有股票的名称和交易信息
#输出：保存到文件中
#候选网站的选取原则：股票信息静态存在于HTML页面中，非js代码生成，没有Robots协议限制
#选取网站：东方财富网，百度股票。
#步骤一：从东方财富网获取股票列表。
#步骤二：根据股票列表逐个到百度股票获取个股信息。
#步骤三：将结果存储到文件

import requests
from bs4 import BeautifulSoup
import traceback
import re

def getHTMLText(url):
    try:
        r = requests.get(url)
        r.raise_for_status()
        r.encoding = r.apparent_encoding
        return r.text
    except:
        return ""

def getStockList(lst, stockURL):  #lst保存的列表类型，里面存储了所有股票的信息，stockURL获得股票列表的url网站
    html = getHTMLText(url)
    soup = Beautifulsoup(html, "parser")
    a = soup.find_all('a')
    for i in a:
        try:
            href = i.attrs['href']
            lst.append(re.findall(r"[s][hz]\d{6}", href)[0])
        except:
            continue


def getStockInfo(lst, stockURL, fpath):   #lst保存所有股票的信息列表，fpath将来要将文件保存的文件路径
    for stock in lst:
        url = stockURL + stock + ".html"
        html = getHTMLText(url)
        try:
            if html == "":
                continue
            infoDict = {}
            soup = BeautifulSoup(html, 'html.parser')
            stockInfo = soup.find('div', attrs={'class':'stock-bets'})

            name = stockInfo.find_all(attrs={'class':'bets-name'})[0]
            infoDict.update({'股票名称':name.text.split()[0]})




def main():
    stock_list_url = 'http://quote.eastmoney.com/stocklist.html' 
    stock_info_url = 'https://gupiao.baidu.com/stock/'
    output_file = '/home/yaoying/py_code/crawler//BaiduStockInfo.txt'
    slist = []
    getStockList(slist, stock_list_url) #获取股票列表
    getStockInfo(slist, stock_info_url, output_file)  #根据股票列表到相关网站上获取股票信息，并把他存储到相关文件中

main()

