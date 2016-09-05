import java.io.IOException;
import java.util.*;
import java.text.*;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.util.Tool;
import org.apache.hadoop.util.ToolRunner;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.input.FileSplit;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;
import org.apache.hadoop.mapreduce.lib.output.MultipleOutputs;
import org.apache.hadoop.mapreduce.lib.partition.*;

public class join extends Configured implements Tool {

  public static class Map extends Mapper<Object, Text, Text, Text> {

    private boolean is_left_rel = false;
    private int index;

    public void setup(Context context)
        throws IOException, InterruptedException {
      String relation = ((FileSplit)context.getInputSplit()).getPath().getParent().getName();
      if (relation.compareTo("left_input512") == 0) {
        is_left_rel = true;
      }
    }

    public void map(Object key, Text value, Context context)
        throws IOException, InterruptedException {
      String[] left_input512 = value.toString().trim().split(" ");
      String[] right_input512 = value.toString().trim().split(" ");

      if (this.is_left_rel) {
        String left_input512_tmp = "";
        for (int left_input512_i = 0; left_input512_i < left_input512.length; left_input512_i++) {
          left_input512_tmp += " " + left_input512[left_input512_i];
        }
        context.write(new Text(left_input512[1]),
                      new Text("L" + left_input512_tmp));
      } else {
        String right_input512_tmp = "";
        for (int right_input512_i = 0; right_input512_i < right_input512.length; right_input512_i++) {
          if (right_input512_i != 1) {
            right_input512_tmp += " " + right_input512[right_input512_i];
          }
        }
        if (right_input512_tmp.isEmpty()) {
          right_input512_tmp = " ";
        }
        context.write(new Text(right_input512[1]),
                      new Text("R" + right_input512_tmp));
      }
    }

    @Override
    public void cleanup(Context context)
      throws IOException, InterruptedException {
    }

  }

  public static class Reduce extends Reducer<Text, Text, NullWritable, Text>{

    private int index;

    public void reduce(Text key, Iterable<Text> values, Context context)
      throws IOException, InterruptedException {
      List<String> arrayLeft = new LinkedList<String>();
      List<String> arrayRight = new LinkedList<String>();
      for (Text text : values) {
        String tmp = text.toString();
        if (tmp.charAt(0) == 'L') {
          arrayLeft.add(tmp.substring(2));
        } else {
          arrayRight.add(tmp.substring(2));
        }
      }
      for (String left : arrayLeft) {
        for (String right : arrayRight) {
          String[] output = (left + " " + right).split(" ");
          String join_output = "";
          for (int output_it = 0; output_it < output.length; output_it++) {
            join_output += output[output_it] + " ";
          }
          context.write(NullWritable.get(), new Text(join_output.trim()));
        }
      }
    }

  }

  public int run(String[] args) throws Exception {
    Configuration conf = new Configuration();
    Job job = new Job(conf, "join");
    job.setJarByClass(join.class);
    job.setMapOutputKeyClass(Text.class);
    job.setMapOutputValueClass(Text.class);
    job.setOutputKeyClass(NullWritable.class);
    job.setOutputValueClass(Text.class);
    job.setMapperClass(Map.class);
    job.setReducerClass(Reduce.class);
    job.setNumReduceTasks(16);
    FileInputFormat.addInputPath(job, new Path("/qjump/left_input512/"));
    FileInputFormat.addInputPath(job, new Path("/qjump/right_input512/"));
    FileOutputFormat.setOutputPath(job, new Path("/qjump/join/"));
    return (job.waitForCompletion(true) ? 0 : 1);
  }

  public static void main(String[] args) throws Exception {
    int res = ToolRunner.run(new Configuration(), new join(), args);
    System.exit(res);
  }

}
