#爬取中国最好大学排名
#程序的结构设计
#步骤一：从网络上获取大学排名网页内容getHTMLText()
#步骤二：提取网页内容中信息到合适的数据结构fillUnivList()
#步骤三：利用数据结构展示并输出结果printUnivList()
import requests
from bs4 import BeautifulSoup
import bs4

def getHTMLText(url):
    try:
        r = requests.get(url, timeout = 30)
        r.raise_for_status()
        r.encoding = r.apparent_encoding
        return r.text
    except:
        return ""

def fillUnivList(ulist, html):
    soup = BeautifulSoup(html, "html.parser")
    for tr in soup.find('tbody').children:
        if isinstance(tr, bs4.element.Tag):
            tds = tr('td')
            ulist.append([tds[0].string, tds[1].string, tds[3].string])

def printUnivList(ulist, num):
    tplt = "{0:^10}\t{1:{3}^10}\t{2:^10}"
    print(tplt.format("排名","学校","得分", chr(12288)))
    for i in range(num):
        u=ulist[i]
        print(tplt.format(u[0], u[1], u[2], chr(12288)))
    

def main():
    uinfo = []
    url = 'http://www.zuihaodaxue.com/zuihaodaxuepaiming2019.html'
    html = getHTMLText(url)
    fillUnivList(uinfo, html)
    printUnivList(uinfo, 20)

if __name__ == '__main__':
    main() 
