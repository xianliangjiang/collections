all:
	javac -cp ext/hadoop-common.jar:ext/hadoop-core.jar:ext/hadoop-hdfs.jar join.java
	jar cf join.jar *.class

run:
	hadoop jar join.jar join

clean:
	rm -f *.class
	rm -f join.jar
