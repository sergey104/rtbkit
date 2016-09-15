# Amazon Kinesis Client Library for Ruby

    require 'aws/kclrb'

    class SampleRecordProcessor < Aws::KCLrb::RecordProcessorBase
      def init_processor(shard_id)
        # initialize
      end

      def process_records(records, checkpointer)
        # process batch of records
      end

      def shutdown(checkpointer, reason)
        # cleanup
      end
    end

    if __FILE__ == $0
      # Start the main processing loop
      record_processor = SampleRecordProcessor.new
      driver = Aws::KCLrb::KCLProcess.new(record_processor)
      driver.run
    end
```

